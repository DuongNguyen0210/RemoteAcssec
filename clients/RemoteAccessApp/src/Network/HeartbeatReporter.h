#ifndef HEARTBEATREPORTER_H
#define HEARTBEATREPORTER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QNetworkReply>

// ---------------------------------------------------------------------------
// HeartbeatReporter
//
// Sends a periodic HTTP POST to the Management API heartbeat endpoint every
// HEARTBEAT_INTERVAL_MS milliseconds while a CHILD device is logged in.
//
// Ownership: parented to AppController so that it survives after LoginWindow.
//
// Refactored to use ApiClient for JWT-based authenticated requests.
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
    // Identity now comes from the JWT stored in ApiClient, so no username is passed.
    // Sends the first heartbeat immediately, then every HEARTBEAT_INTERVAL_MS.
    void start();

    // Stop the timer. Safe to call even if not started.
    void stop();

    bool isRunning() const;

private slots:
    void sendHeartbeat();
    void onHeartbeatReply(QNetworkReply *reply);

private:
    QTimer *m_timer;

    QString m_deviceName;  // QHostInfo::localHostName()
    QString m_deviceUid;   // QSysInfo::machineUniqueId() -> hex, with fallback

    // Resolve a stable device UID from QSysInfo; falls back to hostname if empty.
    QString resolveDeviceUid() const;
};

#endif // HEARTBEATREPORTER_H
