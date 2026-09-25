#pragma once
#include "RelayEndpointProvider.h"
class DeviceService;

class DynamicRelayEndpointProvider final : public RelayEndpointProvider {
public:
    explicit DynamicRelayEndpointProvider(QObject *parent = nullptr);
    void allocate(Callback callback) override;
    void lookup(const QString &agentSessionId, Callback callback) override;
private:
    DeviceService *m_devices;
};
