#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <optional>
#include "RelayEndpointProvider.h"
#include "Network/Protocol/RdtpStreamParser.h"
class RelayClient;

/** Owns child transport, registration, session negotiation and endpoint lease renewal. */
class ChildSessionController : public QObject {
    Q_OBJECT
public:
    static constexpr int DEFAULT_ACK_STALL_INTERVAL_MS = 1500;

    explicit ChildSessionController(RelayEndpointProvider *provider, QObject *parent = nullptr);
    void start();
    void stop();
    bool canSendScreenFrame() const;
    bool sendScreenFrame(quint32 frameId, const QList<QByteArray> &packets);
    qint64 sendScreenPacket(const QByteArray &packet);
    qint64 pendingBytes() const;
    bool isScreenFrameAcksEnabled() const { return m_screenFrameAcksEnabled; }
    void setAckStallIntervalMs(int intervalMs);
    int ackStallIntervalMs() const { return m_frameAckStallTimer.interval(); }
    // Compatibility seam for the unchanged ScreenStreamSender capture loop.
    RelayClient *relayClient() const { return m_client; }
    quint64 activeSessionId() const { return m_sessionId; }
    bool relayReady() const { return m_registered; }
signals:
    void relayRegistered();
    void sessionStarted(quint64 sessionId);
    void sessionEnded(const QString &reason);
    void inputReceived(const Protocol::ProtocolHeader &header, const QByteArray &payload);
    void sessionFailed(const QString &reason);
    void screenFrameAcknowledged(quint32 frameId, qint64 rttMs, qsizetype wireBytes);
    void screenFrameAckStalled(quint32 frameId, qint64 elapsedMs);
private:
    void allocate();
    void connected();
    void receive(const QByteArray &bytes);
    void reset(const QString &reason);
    void fail(const QString &reason);
    void scheduleReconnect();
    bool sendControl(Protocol::MessageType type, quint64 sessionId, const QByteArray &payload = {});
    void clearFrameWindow();
    bool resetRelayState();

    QPointer<RelayEndpointProvider> m_provider;
    RelayClient *m_client;
    QTimer m_retry;
    QTimer m_renew;
    QTimer m_timeout;
    QTimer m_frameAckTimeout;
    QTimer m_frameAckStallTimer;
    QElapsedTimer m_inFlightTimer;
    qsizetype m_inFlightWireBytes = 0;
    Protocol::RdtpStreamParser m_parser;
    RelayEndpoint m_endpoint;
    quint64 m_generation = 0;
    quint64 m_sessionId = 0;
    quint64 m_pendingFrameAckSession = 0;
    std::optional<quint32> m_inFlightFrame;
    bool m_screenFrameAcksEnabled = false;
    bool m_running = false;
    bool m_allocating = false;
    bool m_connecting = false;
    bool m_registered = false;
    int m_retryAttempt = 0;
};
