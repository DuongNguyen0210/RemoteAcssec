#include "HeartbeatReporter.h"
#include "ApiClient.h"

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
    m_deviceName = QHostInfo::localHostName();
    sendHeartbeat();
    m_timer->start();
}

void HeartbeatReporter::stop()
{
    if (m_timer->isActive())
        m_timer->stop();
}

bool HeartbeatReporter::isRunning() const
{
    return m_timer->isActive();
}

void HeartbeatReporter::sendHeartbeat()
{
    if (m_inFlight) return;
    QJsonObject body;
    body[QStringLiteral("hostname")] = m_deviceName;
    body[QStringLiteral("os")] = QSysInfo::prettyProductName();

    QNetworkReply *reply = ApiClient::instance().post("/api/v1/child/heartbeat", body);
    if (!reply) {
        return;
    }
    m_inFlight = true;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHeartbeatReply(reply);
    });
}

void HeartbeatReporter::onHeartbeatReply(QNetworkReply *reply)
{
    m_inFlight = false;
    reply->deleteLater();

    ApiParsedResponse res = ApiClient::parseReply(reply);

    if (res.httpStatusCode == 0) {
        return;
    }

    if (res.success)
        return;

    if (res.httpStatusCode == 404 ||
        res.httpStatusCode == 401 ||
        res.httpStatusCode == 403 ||
        res.httpStatusCode == 400)
    {
        stop();
        emit authenticationLost();
        return;
    }
}
