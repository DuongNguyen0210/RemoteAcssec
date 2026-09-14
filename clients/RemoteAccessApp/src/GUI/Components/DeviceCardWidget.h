#ifndef DEVICECARDWIDGET_H
#define DEVICECARDWIDGET_H

#include <QWidget>
#include <QString>
#include "Domain/Model/DeviceInfo.h"

class DeviceCardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceCardWidget(const DeviceInfo &info, QWidget *parent = nullptr);
    explicit DeviceCardWidget(const QString &agentSessionId, const QString &name,
                              const QString &OS, const QString &ip,
                              const QString &status, const QString &account,
                              QWidget *parent = nullptr);

signals:
    void connectRequested(const QString &agentSessionId);

private:
    QString m_agentSessionId;
};

#endif
