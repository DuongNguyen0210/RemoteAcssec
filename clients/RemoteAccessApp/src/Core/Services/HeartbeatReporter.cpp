#include "HeartbeatReporter.h"
#include "Network/Client/apiclient.h"

#include <QHostInfo>
#include <QSysInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>

HeartbeatReporter::HeartbeatReporter(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(HEARTBEAT_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &HeartbeatReporter::sendHeartbeat);
}

HeartbeatReporter::~HeartbeatReporter()
{
    stop();
}

// ---------------------------------------------------------------------------
// start()
// Caches identity fields, sends the first heartbeat immediately, then starts
// the 5-second repeating timer.
// ---------------------------------------------------------------------------
void HeartbeatReporter::start()
{
    m_deviceName = QHostInfo::localHostName();
    m_deviceUid  = resolveDeviceUid();

    qDebug() << "[HeartbeatReporter] Starting for device=" << m_deviceName
             << " uid=" << m_deviceUid;

    // Send first heartbeat immediately, then repeat every HEARTBEAT_INTERVAL_MS.
    sendHeartbeat();
    m_timer->start();
}

void HeartbeatReporter::stop()
{
    if (m_timer->isActive()) {
        m_timer->stop();
        qDebug() << "[HeartbeatReporter] Timer stopped.";
    }
}

bool HeartbeatReporter::isRunning() const
{
    return m_timer->isActive();
}

// ---------------------------------------------------------------------------
// sendHeartbeat()
// Builds the JSON payload and POSTs it asynchronously via ApiClient.
// ---------------------------------------------------------------------------
void HeartbeatReporter::sendHeartbeat()
{
    QJsonObject body;
    body[QStringLiteral("deviceUid")] = m_deviceUid;
    body[QStringLiteral("name")]      = m_deviceName;
    // username is intentionally omitted: the server fills it from the JWT subject.
    // ipAddress is intentionally omitted: the server fills it from the remote address.

    // Use ApiClient to inject JWT Authorization header automatically.
    QNetworkReply *reply = ApiClient::instance().post("/api/v1/child/heartbeat", body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHeartbeatReply(reply);
    });
}

// ---------------------------------------------------------------------------
// onHeartbeatReply()
// Handles the server response per CLIENT_HEARTBEAT_INTEGRATION.md §5-§6.
//
//  2xx          – success, keep timer running
//  404          – device not found; stop timer (retrying will not help)
//  401/403      – auth failure; log and stop timer
//  400          – client bug; log and keep timer stopped
//  5xx / error  – transient; log and let the next tick retry
// ---------------------------------------------------------------------------
void HeartbeatReporter::onHeartbeatReply(QNetworkReply *reply)
{
    reply->deleteLater();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Network-level failure (no HTTP response at all)
    if (reply->error() != QNetworkReply::NoError && statusCode == 0) {
        qWarning() << "[HeartbeatReporter] Network error:" << reply->errorString()
                   << "– will retry on next tick.";
        return;  // timer continues
    }

    if (statusCode >= 200 && statusCode < 300) {
        qDebug() << "[HeartbeatReporter] Heartbeat OK (HTTP" << statusCode << ")";
        return;  // timer continues
    }

    if (statusCode == 404) {
        const QByteArray body = reply->readAll();
        qWarning() << "[HeartbeatReporter] 404 NOT_FOUND"
                   << "– stopping heartbeat. Body:" << body;
        stop();
        return;
    }

    if (statusCode == 401 || statusCode == 403) {
        const QByteArray body = reply->readAll();
        qWarning() << "[HeartbeatReporter] Auth failure (HTTP" << statusCode << ")"
                   << "– stopping heartbeat. Body:" << body;
        stop();
        return;
    }

    if (statusCode == 400) {
        const QByteArray body = reply->readAll();
        qCritical() << "[HeartbeatReporter] 400 Bad Request – payload invalid"
                    << "Body:" << body;
        stop();
        return;
    }

    // 5xx or any other unexpected code – log and let the next tick retry.
    qWarning() << "[HeartbeatReporter] Unexpected HTTP" << statusCode
               << "– will retry on next tick.";
}

// ---------------------------------------------------------------------------
// resolveDeviceUid()
// Returns a stable, hex-encoded machine identifier.
// ---------------------------------------------------------------------------
QString HeartbeatReporter::resolveDeviceUid() const
{
    const QByteArray raw = QSysInfo::machineUniqueId();
    if (!raw.isEmpty()) {
        return QString::fromLatin1(raw.toHex());
    }

    // Fallback: hostname is deterministic on a given machine.
    const QString fallback = QHostInfo::localHostName();
    qWarning() << "[HeartbeatReporter] QSysInfo::machineUniqueId() returned empty."
               << "Falling back to hostname as deviceUid:" << fallback;
    return fallback;
}
