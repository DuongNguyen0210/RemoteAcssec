#include "DeviceSseStream.h"
#include "ApiClient.h"
#include "Domain/Model/DeviceInfo.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>
#include <QUrl>

DeviceSseStream::DeviceSseStream(QObject *parent)
    : QObject(parent)
    , m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(RECONNECT_DELAY_MS);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DeviceSseStream::openConnection);
}

DeviceSseStream::~DeviceSseStream()
{
    stop();
}

void DeviceSseStream::start()
{
    if (m_running) return;
    m_running = true;
    openConnection();
}

void DeviceSseStream::stop()
{
    m_running = false;
    m_reconnectTimer->stop();
    abortReply();
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void DeviceSseStream::openConnection()
{
    if (!m_running) return;

    abortReply(); // ensure previous reply is gone
    m_buffer.clear();

    // Build the SSE request: Accept text/event-stream, no Transfer-Timeout
    // (we want a long-lived connection).
    QString token = ApiClient::instance().getToken();
    QUrl url(qEnvironmentVariable("REMOTE_API_URL",
                                   "http://cornball-ibuprofen-polygraph.ngrok-free.dev")
              + QStringLiteral("/api/v1/devices/events"));

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "text/event-stream");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("ngrok-skip-browser-warning", "true");
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    }
    // Disable Qt's default transfer timeout — SSE is an intentionally long stream
    request.setTransferTimeout(0);

    m_reply = ApiClient::instance().getManager()->get(request);
    if (!m_reply) {
        qWarning() << "[SSE] Failed to create network reply, will retry";
        scheduleReconnect();
        return;
    }

    connect(m_reply, &QNetworkReply::readyRead, this, &DeviceSseStream::onReadyRead);
    connect(m_reply, &QNetworkReply::finished,  this, &DeviceSseStream::onFinished);

    qDebug() << "[SSE] Connecting to device event stream";
}

void DeviceSseStream::onReadyRead()
{
    if (!m_reply) return;

    // Check HTTP status on first data
    int httpStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus != 0 && httpStatus != 200) {
        qWarning() << "[SSE] Server returned HTTP" << httpStatus << "- stopping";
        stop();
        return;
    }

    m_buffer.append(m_reply->readAll());
    parseBuffer();
}

void DeviceSseStream::onFinished()
{
    if (!m_reply) return;

    QNetworkReply::NetworkError err = m_reply->error();
    m_reply->deleteLater();
    m_reply = nullptr;

    emit disconnected();

    if (!m_running) return; // stop() was called

    if (err != QNetworkReply::NoError && err != QNetworkReply::RemoteHostClosedError) {
        qWarning() << "[SSE] Connection error:" << err << "- scheduling reconnect";
    } else {
        qDebug() << "[SSE] Stream closed by server - scheduling reconnect";
    }

    scheduleReconnect();
}

void DeviceSseStream::parseBuffer()
{
    // SSE format (per spec):
    //   event: <name>\n
    //   data: <payload>\n
    //   \n                   <- blank line marks end of event
    //
    // We collect lines between blank lines into an event block.

    while (true) {
        // Find double-newline (event boundary)
        int boundary = m_buffer.indexOf("\n\n");
        if (boundary == -1) break; // incomplete event - wait for more data

        QByteArray eventBlock = m_buffer.left(boundary);
        m_buffer.remove(0, boundary + 2); // consume up to and including \n\n

        QString eventName;
        QByteArray dataPayload;

        for (const QByteArray &rawLine : eventBlock.split('\n')) {
            QByteArray line = rawLine.trimmed();
            if (line.startsWith("event:")) {
                eventName = QString::fromUtf8(line.mid(6).trimmed());
            } else if (line.startsWith("data:")) {
                // Accumulate data lines (spec allows multiple)
                if (!dataPayload.isEmpty()) dataPayload.append('\n');
                dataPayload.append(line.mid(5).trimmed());
            }
            // id: and retry: lines are intentionally ignored here
        }

        if (eventName != QLatin1String("device-list") || dataPayload.isEmpty()) {
            // Unknown or empty event - skip
            if (!eventName.isEmpty())
                qDebug() << "[SSE] Ignoring event:" << eventName;
            continue;
        }

        // Parse JSON array of DeviceInfo
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(dataPayload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
            qWarning() << "[SSE] Failed to parse device-list JSON:" << parseError.errorString();
            continue;
        }

        QList<DeviceInfo> devices;
        for (const QJsonValue &val : doc.array()) {
            QJsonObject obj = val.toObject();
            DeviceInfo d;
            d.sessionId       = obj.value(QStringLiteral("sessionId")).toString();
            d.childId         = obj.value(QStringLiteral("childId")).toVariant().toLongLong();
            d.username        = obj.value(QStringLiteral("username")).toString();
            d.deviceName      = obj.value(QStringLiteral("deviceName")).toString();
            d.os              = obj.value(QStringLiteral("os")).toString();
            d.ipAddress       = obj.value(QStringLiteral("ipAddress")).toString();
            d.lastHeartbeatAt = obj.value(QStringLiteral("lastHeartbeatAt")).toVariant().toLongLong();
            if (!d.sessionId.isEmpty())
                devices.append(d);
        }

        qDebug() << "[SSE] Received device-list update:" << devices.size() << "devices";
        emit devicesReceived(devices);

        // Emit connected() only on the very first successful event
        static bool firstEvent = true;
        if (firstEvent) {
            firstEvent = false;
            emit connected();
        }
    }
}

void DeviceSseStream::scheduleReconnect()
{
    if (m_running && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void DeviceSseStream::abortReply()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}
