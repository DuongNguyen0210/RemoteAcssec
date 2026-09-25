#pragma once
#include <QObject>
#include "Network/Relay/RelayEndpointProvider.h"
class QNetworkReply;
#include <QList>
#include "Domain/Model/DeviceInfo.h"

class DeviceService : public QObject {
    Q_OBJECT
public:
    explicit DeviceService(QObject *parent = nullptr) : QObject(parent) {}
    void fetchDevices();
    void allocateRelay(RelayEndpointProvider::Callback callback);
    void fetchRelayEndpoint(const QString &agentSessionId, RelayEndpointProvider::Callback callback);
signals:
    void devicesResult(bool success, const QList<DeviceInfo>& devices, const QString& message);
private:
    void receiveEndpoint(QNetworkReply *reply, RelayEndpointProvider::Callback callback);
    bool m_loading = false;
};
