#include "DynamicRelayEndpointProvider.h"
#include "Network/Http/DeviceService.h"

DynamicRelayEndpointProvider::DynamicRelayEndpointProvider(QObject *parent)
    : RelayEndpointProvider(parent), m_devices(new DeviceService(this)) {}

void DynamicRelayEndpointProvider::allocate(Callback callback) { m_devices->allocateRelay(std::move(callback)); }
void DynamicRelayEndpointProvider::lookup(const QString &id, Callback callback) {
    m_devices->fetchRelayEndpoint(id, std::move(callback));
}
