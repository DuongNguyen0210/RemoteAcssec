#ifndef HEARTBEATREPORTER_H
#define HEARTBEATREPORTER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// ---------------------------------------------------------------------------
// HeartbeatReporter
//
// Sends a periodic HTTP POST to the Management API heartbeat endpoint every
// HEARTBEAT_INTERVAL_MS milliseconds while a CHILD device is logged in.
//
// Ownership: must be parented to a long-lived object (e.g. qApp) so that it
// survives after LoginWindow is closed.
//
// Technical-debt note: the heartbeat base URL is isolated here and NOT merged
// with Authservice::API_URL to protect the currently working login code.
// A shared ApiConfig class should be introduced in a future refactor to
// eliminate this duplication.
// ---------------------------------------------------------------------------

class HeartbeatReporter : public QObject
{
    Q_OBJECT

public:
    // Interval between successive heartbeat requests (milliseconds).
    static constexpr int HEARTBEAT_INTERVAL_MS = 5000;

    explicit HeartbeatReporter(QObject *parent = nullptr);
    ~HeartbeatReporter();

    // Start sending heartbeats.
    // username – the child_username that returned role=CHILD from login.
    // Sends the first heartbeat immediately, then every HEARTBEAT_INTERVAL_MS.
    void start(const QString &username);

    // Stop the timer. Safe to call even if not started.
    void stop();

    bool isRunning() const;

private slots:
    void sendHeartbeat();
    void onHeartbeatReply(QNetworkReply *reply);

private:
    // ---------------------------------------------------------------------------
    // TECHNICAL DEBT: This base URL is isolated here and intentionally NOT
    // merged with Authservice::API_URL to protect the currently working login
    // code.  A shared ApiConfig class should be introduced in a future refactor
    // so both classes share the same host/port configuration.
    //
    // The Management API runs on port 9090 per application.yml.
    // See: docs/telemetry/CLIENT_HEARTBEAT_INTEGRATION.md – section 1.
    // ---------------------------------------------------------------------------
    static const QString HEARTBEAT_BASE_URL;  // "http://localhost:9090"
    static const QString HEARTBEAT_ENDPOINT;  // "/api/v1/devices/heartbeat"

    QTimer                *m_timer;
    QNetworkAccessManager *m_networkManager;

    QString m_username;    // authenticated child_username
    QString m_deviceName;  // QHostInfo::localHostName()
    QString m_deviceUid;   // QSysInfo::machineUniqueId() -> hex, with fallback

    // Resolve a stable device UID from QSysInfo; falls back to hostname if empty.
    QString resolveDeviceUid() const;
};

#endif // HEARTBEATREPORTER_H
