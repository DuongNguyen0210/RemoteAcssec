#ifndef DEVICECARDWIDGET_H
#define DEVICECARDWIDGET_H

#include <QWidget>
#include <QString>
#include "Domain/Model/DeviceInfo.h"

class QPushButton;

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
    QString m_agentSessionId;
    QPushButton *m_connectButton;
};

#endif
