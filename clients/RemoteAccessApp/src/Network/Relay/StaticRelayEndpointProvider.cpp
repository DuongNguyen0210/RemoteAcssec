#include "StaticRelayEndpointProvider.h"

StaticRelayEndpointProvider::StaticRelayEndpointProvider(QObject *parent)
    : StaticRelayEndpointProvider(qEnvironmentVariable("REMOTE_RELAY_HOST", "localhost"),
            qEnvironmentVariable("REMOTE_RELAY_PORT", "8080").toUShort(), parent) {}

StaticRelayEndpointProvider::StaticRelayEndpointProvider(const QString &host, quint16 port, QObject *parent)
    : RelayEndpointProvider(parent), m_endpoint{QStringLiteral("static"), host, port, 0} {}

void StaticRelayEndpointProvider::allocate(Callback callback) {
    callback(m_endpoint, m_endpoint.isValid() ? QString() : QStringLiteral("Cấu hình Relay không hợp lệ."));
}

void StaticRelayEndpointProvider::lookup(const QString &, Callback callback) { allocate(std::move(callback)); }
