#include "HeartbeatReporter.h"

#include <QHostInfo>
#include <QSysInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>

// ---------------------------------------------------------------------------
// Static constants
//
// TECHNICAL DEBT: HEARTBEAT_BASE_URL is hardcoded here separately from
// Authservice::API_URL (which targets an ngrok tunnel).  The Management API
// base URL (host + port) should be centralised in a shared ApiConfig class
// in a future refactor.  Port 9090 is taken from application.yml as
// documented in docs/telemetry/CLIENT_HEARTBEAT_INTEGRATION.md §1.
// ---------------------------------------------------------------------------
const QString HeartbeatReporter::HEARTBEAT_BASE_URL = QStringLiteral("http://localhost:9090");
const QString HeartbeatReporter::HEARTBEAT_ENDPOINT = QStringLiteral("/api/v1/devices/heartbeat");

HeartbeatReporter::HeartbeatReporter(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_networkManager(new QNetworkAccessManager(this))
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
void HeartbeatReporter::start(const QString &username)
{
    if (username.trimmed().isEmpty()) {
        qWarning() << "[HeartbeatReporter] start() called with empty username – aborting.";
        return;
    }

    m_username   = username.trimmed();
    m_deviceName = QHostInfo::localHostName();
    m_deviceUid  = resolveDeviceUid();

    qDebug() << "[HeartbeatReporter] Starting for user=" << m_username
             << " device=" << m_deviceName
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
// Builds the JSON payload and POSTs it asynchronously.
// Contract: docs/telemetry/CLIENT_HEARTBEAT_INTEGRATION.md §2-§3
// ---------------------------------------------------------------------------
void HeartbeatReporter::sendHeartbeat()
{
    QJsonObject body;
    body[QStringLiteral("deviceUid")] = m_deviceUid;
    body[QStringLiteral("name")]      = m_deviceName;
    body[QStringLiteral("username")]  = m_username;
    // ipAddress is intentionally omitted: the server fills it from the
    // TCP remote address (HttpServletRequest.getRemoteAddr()).
    // See CLIENT_HEARTBEAT_INTEGRATION.md §3 and §9.

    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkRequest request;
    request.setUrl(QUrl(HEARTBEAT_BASE_URL + HEARTBEAT_ENDPOINT));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QByteArrayLiteral("application/json"));

    QNetworkReply *reply = m_networkManager->post(request, payload);
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
        qDebug() << "[HeartbeatReporter] Heartbeat OK (HTTP" << statusCode << ") user=" << m_username;
        return;  // timer continues
    }

    if (statusCode == 404) {
        // Device not found – retrying will not help until the account is provisioned.
        const QByteArray body = reply->readAll();
        qWarning() << "[HeartbeatReporter] 404 DEVICE_NOT_FOUND for user=" << m_username
                   << "– stopping heartbeat. Body:" << body;
        stop();
        return;
    }

    if (statusCode == 400) {
        // Validation error – this is a client bug; log and stop to avoid flooding.
        const QByteArray body = reply->readAll();
        qCritical() << "[HeartbeatReporter] 400 Bad Request – payload invalid for user="
                    << m_username << "Body:" << body;
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
// If QSysInfo::machineUniqueId() is empty (e.g. some Linux configurations),
// falls back to the hostname.  The fallback is clearly reported in the log.
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
