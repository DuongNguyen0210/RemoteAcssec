#ifndef DEVICESCONTROLLER_H
#define DEVICESCONTROLLER_H

#include <QObject>
#include <QString>
#include "Domain/Model/DeviceInfo.h"

class DevicesPage;
class DeviceStore;
class AccountService;
class AdminSessionController;

class DevicesController : public QObject
{
    Q_OBJECT
public:
    explicit DevicesController(DeviceStore *store, QObject *parent = nullptr);
    ~DevicesController();

    DevicesPage* getView() const;
    void refresh();

signals:
    void remoteSessionStarted(quint64 sessionId, const QString &childUsername);

private slots:
    void onDevicesUpdated(const QList<DeviceInfo> &devices);
    void onConnectRequested(const QString &childUsername);
    void onRemoveDeviceRequested(const QString &childUsername);
    void handleDeviceRemoved(bool success, const QString &childUsername, const QString &message);
    void handleSessionEstablished(quint64 sessionId);
    void handleSessionFailed(const QString &reason);

private:
    DevicesPage *m_view;
    DeviceStore *m_store;
    AccountService *m_accountService;
    AdminSessionController *m_sessionController;
    QString m_connectingChildUsername;
};

#endif
