#ifndef HEARTBEATREPORTER_H
#define HEARTBEATREPORTER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QNetworkReply>
#include "Utils/SystemTelemetrySampler.h"

class HeartbeatReporter : public QObject
{
    Q_OBJECT

public:
    static constexpr int HEARTBEAT_INTERVAL_MS = 2000;

    explicit HeartbeatReporter(QObject *parent = nullptr);
    ~HeartbeatReporter();

    void start();
    void stop();

    bool isRunning() const;

signals:
    void authenticationLost();

private slots:
    void sendHeartbeat();
    void sendUnshare();
    void onHeartbeatReply(QNetworkReply *reply);

private:
    QTimer *m_timer;

    QString m_deviceName;
    SystemTelemetrySampler m_telemetry;
    qint64 m_sampleSequence = 0;
    bool m_inFlight = false;
    bool m_unsharePending = false;

};

#endif
