#ifndef DEVICESCONTROLLER_H
#define DEVICESCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QString>
#include "Domain/Model/DeviceInfo.h"

class DevicesPage;
class DeviceStore;
class DeviceService;
class AdminSessionController;

class DevicesController : public QObject
{
    Q_OBJECT
public:
    explicit DevicesController(DeviceStore *store, DeviceService *service, QObject *parent = nullptr);
    ~DevicesController();

    DevicesPage* getView() const;
    void refresh();

signals:
    void remoteSessionStarted(quint64 sessionId, const QString &agentSessionId);

private slots:
    void onDevicesUpdated(const QList<DeviceInfo> &devices);
    void onConnectRequested(const QString &agentSessionId);
    void handleSessionEstablished(quint64 sessionId);
    void handleSessionFailed(const QString &reason);

private:
    QPointer<DevicesPage> m_view;
    DeviceStore *m_store;
    DeviceService *m_deviceService;
    AdminSessionController *m_sessionController;
    QString m_connectingAgentSessionId;
};

#endif
