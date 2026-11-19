#include "HeartbeatReporter.h"
#include "ApiClient.h"
#include "TelemetryJson.h"

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

void HeartbeatReporter::start()
{
    if (m_timer->isActive()) return;
    m_telemetry.reset();
    m_deviceName = QHostInfo::localHostName();
    m_timer->start();
    sendHeartbeat();
}

void HeartbeatReporter::stop()
{
    if (!m_timer->isActive()) return;
    m_timer->stop();
    m_unsharePending = true;
    sendUnshare();
}

bool HeartbeatReporter::isRunning() const
{
    return m_timer->isActive();
}

void HeartbeatReporter::sendHeartbeat()
{
    if (m_inFlight || !m_timer->isActive()) return;
    if (m_unsharePending) { sendUnshare(); return; }
    QJsonObject body;
    body[QStringLiteral("hostname")] = m_deviceName;
    body[QStringLiteral("os")] = QSysInfo::prettyProductName();
    body[QStringLiteral("telemetry")] = TelemetryJson::encode(m_telemetry.sample(), ++m_sampleSequence);

    QNetworkReply *reply = ApiClient::instance().post("/api/v1/child/heartbeat", body);
    if (!reply) {
        return;
    }
    m_inFlight = true;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHeartbeatReply(reply);
    });
}

void HeartbeatReporter::sendUnshare()
{
    if (m_inFlight || !m_unsharePending) return;
    auto *reply = ApiClient::instance().post("/api/v1/child/unshare", {});
    if (!reply) return; // Presence TTL remains the network-failure fallback.
    m_inFlight = true;
    m_unsharePending = false;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_inFlight = false;
        reply->deleteLater();
        if (m_unsharePending) sendUnshare();
        else if (m_timer->isActive()) sendHeartbeat();
    });
}

void HeartbeatReporter::onHeartbeatReply(QNetworkReply *reply)
{
    m_inFlight = false;
    reply->deleteLater();
    const auto res = ApiClient::parseReply(reply);
    if (!res.success && (res.httpStatusCode == 404 || res.httpStatusCode == 401
                        || res.httpStatusCode == 403 || res.httpStatusCode == 400)) {
        m_timer->stop();
        m_unsharePending = false;
        emit authenticationLost();
        return;
    }
    // Serialize stop after any heartbeat already in flight, and resume after unshare.
    if (m_unsharePending) sendUnshare();
}
