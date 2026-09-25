#pragma once
#include "RelayEndpointProvider.h"

class StaticRelayEndpointProvider final : public RelayEndpointProvider {
public:
    explicit StaticRelayEndpointProvider(QObject *parent = nullptr);
    StaticRelayEndpointProvider(const QString &host, quint16 port, QObject *parent = nullptr);
    void allocate(Callback callback) override;
    void lookup(const QString &agentSessionId, Callback callback) override;
private:
    RelayEndpoint m_endpoint;
};
