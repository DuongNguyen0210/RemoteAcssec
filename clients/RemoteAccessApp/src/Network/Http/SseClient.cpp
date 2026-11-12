#include "SseClient.h"
#include "ApiClient.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDebug>

SseClient::SseClient(const QString &endpoint, QObject *parent)
    : QObject(parent)
    , m_endpoint(endpoint)
    , m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(DEFAULT_RECONNECT_DELAY_MS);
    connect(m_reconnectTimer, &QTimer::timeout, this, &SseClient::openConnection);
}

SseClient::~SseClient()
{
    stop();
}

void SseClient::setEndpoint(const QString &endpoint)
{
    if (m_endpoint == endpoint) return;
    const bool wasRunning = m_running;
    stop();
    m_endpoint = endpoint;
    if (wasRunning) start();
}

void SseClient::start()
{
    if (m_running) return;
    m_running = true;
    m_hasEmittedConnected = false;
    openConnection();
}

void SseClient::stop()
{
    m_running = false;
    m_hasEmittedConnected = false;
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    abortReply();
}

void SseClient::openConnection()
{
    if (!m_running) return;

    abortReply();
    m_buffer.clear();

    QString token = ApiClient::instance().getToken();
    QString baseUrl = qEnvironmentVariable("REMOTE_API_URL",
                                           "http://cornball-ibuprofen-polygraph.ngrok-free.dev");
    QUrl url(baseUrl + m_endpoint);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "text/event-stream");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("ngrok-skip-browser-warning", "true");
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    }
    // Infinite timeout for SSE stream
    request.setTransferTimeout(0);

    auto *manager = ApiClient::instance().getManager();
    if (!manager) {
        qWarning() << "[SseClient] No QNetworkAccessManager available";
        scheduleReconnect();
        return;
    }

    m_reply = manager->get(request);
    if (!m_reply) {
        qWarning() << "[SseClient] Failed to send GET request to" << m_endpoint;
        scheduleReconnect();
        return;
    }

    connect(m_reply.data(), &QNetworkReply::readyRead, this, &SseClient::onReadyRead);
    connect(m_reply.data(), &QNetworkReply::finished,  this, &SseClient::onFinished);

    qDebug() << "[SseClient] Connecting to SSE endpoint:" << m_endpoint;
}

void SseClient::onReadyRead()
{
    if (!m_reply) return;
    QPointer<QNetworkReply> reply = m_reply;

    int httpStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus != 0 && httpStatus != 200) {
        qWarning() << "[SseClient] Server returned status" << httpStatus << "for" << m_endpoint;
        stop();
        return;
    }

    if (!m_hasEmittedConnected) {
        m_hasEmittedConnected = true;
        emit connected();
        if (!reply || m_reply != reply) return;
    }

    m_buffer.append(reply->readAll());
    parseBuffer();
}

void SseClient::onFinished()
{
    if (!m_reply) return;

    QNetworkReply::NetworkError err = m_reply->error();
    m_reply->deleteLater();
    m_reply = nullptr;

    m_hasEmittedConnected = false;
    emit disconnected();

    if (!m_running) return;

    if (err != QNetworkReply::NoError && err != QNetworkReply::RemoteHostClosedError) {
        qWarning() << "[SseClient] Connection error on" << m_endpoint << ":" << err << "- will reconnect";
    } else {
        qDebug() << "[SseClient] Stream disconnected normally on" << m_endpoint << "- will reconnect";
    }

    scheduleReconnect();
}

void SseClient::parseBuffer()
{
    // SSE spec: events end with "\n\n" (or "\r\n\r\n")
    while (true) {
        int delimiterPos = -1;
        int delimiterLen = 0;

        int crlfPos = m_buffer.indexOf("\r\n\r\n");
        int lfPos   = m_buffer.indexOf("\n\n");

        if (crlfPos != -1 && (lfPos == -1 || crlfPos < lfPos)) {
            delimiterPos = crlfPos;
            delimiterLen = 4;
        } else if (lfPos != -1) {
            delimiterPos = lfPos;
            delimiterLen = 2;
        }

        if (delimiterPos == -1) {
            break; // Incomplete block, wait for more data
        }

        QByteArray eventBlock = m_buffer.left(delimiterPos);
        m_buffer.remove(0, delimiterPos + delimiterLen);

        QString eventName;
        QByteArray dataPayload;

        for (const QByteArray &rawLine : eventBlock.split('\n')) {
            QByteArray line = rawLine;
            if (line.endsWith('\r')) line.chop(1);
            line = line.trimmed();

            if (line.isEmpty() || line.startsWith(':')) {
                // Comment or empty line -> ignore per SSE spec
                continue;
            }

            if (line.startsWith("event:")) {
                eventName = QString::fromUtf8(line.mid(6).trimmed());
            } else if (line.startsWith("data:")) {
                if (!dataPayload.isEmpty()) {
                    dataPayload.append('\n');
                }
                dataPayload.append(line.mid(5).trimmed());
            }
        }

        if (!dataPayload.isEmpty() || !eventName.isEmpty()) {
            emit eventReceived(eventName, dataPayload);
        }
    }
}

void SseClient::scheduleReconnect()
{
    if (m_running && m_reconnectTimer && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void SseClient::abortReply()
{
    QPointer<QNetworkReply> reply = m_reply;
    m_reply = nullptr;
    if (!reply) return;
    disconnect(reply.data(), nullptr, this, nullptr);
    reply->abort();
    if (reply) reply->deleteLater();
}
