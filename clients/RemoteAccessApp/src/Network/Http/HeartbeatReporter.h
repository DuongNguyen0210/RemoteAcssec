#ifndef HEARTBEATREPORTER_H
#define HEARTBEATREPORTER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QNetworkReply>

class HeartbeatReporter : public QObject
{
    Q_OBJECT

public:
    static constexpr int HEARTBEAT_INTERVAL_MS = 5000;

    explicit HeartbeatReporter(QObject *parent = nullptr);
    ~HeartbeatReporter();

    void start();
    void stop();

    bool isRunning() const;

private slots:
    void sendHeartbeat();
    void onHeartbeatReply(QNetworkReply *reply);

private:
    QTimer *m_timer;

    QString m_deviceName;
    QString m_deviceUid;

    QString resolveDeviceUid() const;
};

#endif
