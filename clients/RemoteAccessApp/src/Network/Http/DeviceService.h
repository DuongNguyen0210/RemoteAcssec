#pragma once
#include <QObject>
#include "Network/Relay/RelayEndpointProvider.h"
class QNetworkReply;
#include <QList>
#include "Domain/Model/DeviceInfo.h"

class DeviceSseStream;

class DeviceService : public QObject {
    Q_OBJECT
public:
    explicit DeviceService(QObject *parent = nullptr);

    /** One-shot fetch (used for initial load and manual refresh). */
    void fetchDevices();

    /** Opens the SSE stream for real-time push updates. */
    void startSseStream();

    /** Closes the SSE stream (call on logout or shutdown). */
    void stopSseStream();

    void allocateRelay(RelayEndpointProvider::Callback callback);
    void fetchRelayEndpoint(const QString &agentSessionId, RelayEndpointProvider::Callback callback);

signals:
    /**
     * Emitted both by fetchDevices() and by the SSE stream.
     * Connect to DeviceStore::replaceDevices (via DevicesController).
     */
    void devicesResult(bool success, const QList<DeviceInfo>& devices, const QString& message);

private:
    void receiveEndpoint(QNetworkReply *reply, RelayEndpointProvider::Callback callback);

    bool m_loading = false;
    DeviceSseStream *m_sseStream = nullptr;
};
