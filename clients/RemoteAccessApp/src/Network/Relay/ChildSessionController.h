#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include "RelayEndpointProvider.h"
#include "Network/Protocol/RdtpStreamParser.h"
class RelayClient;

/** Owns child transport, registration, session negotiation and endpoint lease renewal. */
class ChildSessionController : public QObject {
    Q_OBJECT
public:
    explicit ChildSessionController(RelayEndpointProvider *provider, QObject *parent = nullptr);
    void start();
    void stop();
    qint64 sendScreenPacket(const QByteArray &packet);
    qint64 pendingBytes() const;
    // Compatibility seam for the unchanged ScreenStreamSender capture loop.
    RelayClient *relayClient() const { return m_client; }
    quint64 activeSessionId() const { return m_sessionId; }
signals:
    void sessionStarted(quint64 sessionId);
    void sessionEnded();
    void inputReceived(const Protocol::ProtocolHeader &header, const QByteArray &payload);
    void sessionFailed(const QString &reason);
private:
    void allocate();
    void connected();
    void receive(const QByteArray &bytes);
    void reset();
    void fail(const QString &reason);
    bool sendControl(Protocol::MessageType type, quint64 sessionId, const QByteArray &payload = {});
    QPointer<RelayEndpointProvider> m_provider;
    RelayClient *m_client;
    QTimer m_retry;
    QTimer m_renew;
    QTimer m_timeout;
    Protocol::RdtpStreamParser m_parser;
    RelayEndpoint m_endpoint;
    quint64 m_generation = 0;
    quint64 m_sessionId = 0;
    bool m_running = false;
    bool m_allocating = false;
    bool m_connecting = false;
    bool m_registered = false;
};
