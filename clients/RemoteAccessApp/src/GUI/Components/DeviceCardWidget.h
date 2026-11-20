#ifndef DEVICECARDWIDGET_H
#define DEVICECARDWIDGET_H

#include <QWidget>
#include <QString>
#include <QElapsedTimer>
#include "Domain/Model/DeviceInfo.h"

class QPushButton;
class QLabel;

class DeviceCardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceCardWidget(const DeviceInfo &info, QWidget *parent = nullptr);
    explicit DeviceCardWidget(const QString &agentSessionId, const QString &name,
                              const QString &OS, const QString &ip,
                              const QString &status, const QString &account,
                              QWidget *parent = nullptr);

    void setPendingConnection(const QString &targetSessionId);

signals:
    void connectRequested(const QString &agentSessionId);

private:
    void refreshTelemetry();
    std::optional<TelemetryInfo> m_telemetry;
    QElapsedTimer m_telemetryElapsed;
    QLabel *m_cpuValue, *m_memoryValue, *m_rxValue, *m_txValue, *m_telemetryState;
    QString m_agentSessionId;
    QPushButton *m_connectButton;
};

#endif
