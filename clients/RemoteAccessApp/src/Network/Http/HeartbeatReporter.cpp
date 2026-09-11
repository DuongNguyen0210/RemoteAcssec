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
    m_deviceUid  = resolveDeviceUid();

    qDebug() << "[HeartbeatReporter] Starting for device=" << m_deviceName
             << " uid=" << m_deviceUid;

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

void HeartbeatReporter::sendHeartbeat()
{
    QJsonObject body;
    body[QStringLiteral("deviceUid")] = m_deviceUid;
    body[QStringLiteral("name")]      = m_deviceName;
    body[QStringLiteral("os")]        = QSysInfo::prettyProductName();

    QNetworkReply *reply = ApiClient::instance().post("/api/v1/child/heartbeat", body);
    if (!reply) {
        return;
    }
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHeartbeatReply(reply);
    });
}

void HeartbeatReporter::onHeartbeatReply(QNetworkReply *reply)
{
    reply->deleteLater();

    ApiParsedResponse res = ApiClient::parseReply(reply);

    if (res.httpStatusCode == 0) {
        return;
    }

    if (res.success) {
        qDebug() << "[HeartbeatReporter] Heartbeat (HTTP" << res.httpStatusCode << "):" << res.message;
        return;
    }

    if (res.httpStatusCode == 404 || res.httpStatusCode == 401 || res.httpStatusCode == 403 || res.httpStatusCode == 400) {
        qWarning() << "[HeartbeatReporter] Stopping heartbeat. Status:" << res.httpStatusCode << "Message:" << res.message;
        stop();
        return;
    }

    qWarning() << "[HeartbeatReporter] Unexpected HTTP" << res.httpStatusCode;
}

QString HeartbeatReporter::resolveDeviceUid() const
{
    const QByteArray raw = QSysInfo::machineUniqueId();
    if (!raw.isEmpty()) {
        return QString::fromLatin1(raw.toHex());
    }

    const QString fallback = QHostInfo::localHostName();
    qWarning() << "[HeartbeatReporter] QSysInfo::machineUniqueId() returned empty."
               << "Falling back to hostname as deviceUid:" << fallback;
    return fallback;
}
