#ifndef DEVICESCONTROLLER_H
#define DEVICESCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QString>
#include "Domain/Model/DeviceInfo.h"

class QImage;
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
    void disconnectCurrentSession();

signals:
    void remoteFrameReady(quint64 sessionId, const QImage &image);
    void remoteSessionEnded();
    void remoteSessionStarted(quint64 sessionId, const QString &agentSessionId);
    void frameReceived(quint32 frameId, const QByteArray &jpegData);
    void sessionEnded(quint64 sessionId, const QString &agentSessionId, const QString &reason);

private slots:
    void onDevicesUpdated(const QList<DeviceInfo> &devices);
    void onConnectRequested(const QString &agentSessionId);
    void handleSessionEstablished(quint64 remoteSessionId, const QString &agentSessionId);
    void handleSessionFailed(const QString &reason);

private:
    QPointer<DevicesPage> m_view;
    DeviceStore *m_store;
    DeviceService *m_deviceService;
    AdminSessionController *m_sessionController;
    bool m_requestedDisconnect = false;
};

#endif
