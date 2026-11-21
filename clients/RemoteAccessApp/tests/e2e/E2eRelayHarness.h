#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTimer>

#include <memory>
#include <random>

#include "Network/Protocol/ProtocolConstants.h"
#include "Network/Protocol/ProtocolHeader.h"
#include "Network/Protocol/RdtpStreamParser.h"

class QTcpSocket;
class ChildSessionController;
class AdminSessionController;
class ScreenStreamSender;

namespace E2eHarness {

struct NetworkProfile
{
    double bandwidthBps = 0.0;            // Bytes/sec (<= 0 means unlimited)
    int baseRttMs = 0;                    // Total round-trip propagation delay (ms)
    int jitterMs = 0;                     // Deterministic one-way jitter upper bound [0..jitterMs]
    int ackDelayMs = 0;                   // Extra delay added to SCREEN_FRAME_ACK (flags == 0)
    bool dropAcks = false;                // Drop all SCREEN_FRAME_ACK (flags == 0) frames
    QSet<quint32> chunkDropIndices;       // Drop SCREEN_FRAME packets matching chunkIndex
    double lossPercent = 0.0;             // Deterministic pseudo-random drop % for SCREEN_FRAME [0..100]
    bool simulateUnwritableAdmin = false; // Emulate !target.isWritable() 1-packet reservation
    bool ackCapabilityEnabled = true;     // Negotiate SCREEN_FRAME_ACK (flags == 1)
    qint64 readBufferLimitBytes = 0;      // 0 = unlimited; > 0 limits child socket read buffer & queue
};

struct WirePacketRecord
{
    qint64 timestampMs = 0;
    bool fromChild = false;
    Protocol::ProtocolHeader header{Protocol::MessageType::HEARTBEAT};
    qsizetype wireSize = 0;
    quint32 frameId = 0;
    quint32 chunkIndex = 0;
    quint32 chunkCount = 0;
    quint32 totalFrameSize = 0;
};

class SimulatedRdtpRelay : public QObject
{
    Q_OBJECT

public:
    static constexpr qsizetype MAX_RESERVED_WIRE_BYTES = 262144; // 256 KiB (RelayServerHandler)
    static constexpr qsizetype MAX_CONTROL_PAYLOAD_BYTES = 8192; // ProtocolDecoder

    explicit SimulatedRdtpRelay(QObject *parent = nullptr);
    ~SimulatedRdtpRelay() override;

    bool start(quint16 port = 0);
    void stop();
    quint16 port() const;

    NetworkProfile networkProfile() const { return m_profile; }
    void setNetworkProfile(const NetworkProfile &profile);
    void scheduleProfileChange(int delayMs, const NetworkProfile &profile);

    QList<WirePacketRecord> recordedPackets() const { return m_recordedPackets; }
    void clearRecordedPackets();

    int protocolViolationCount() const { return m_protocolViolations.size(); }
    QStringList protocolViolations() const { return m_protocolViolations; }
    int overBudgetPacketCount() const { return m_overBudgetPacketCount; }
    int maxInFlightScreenFrames() const { return m_maxInFlightScreenFrames; }
    int droppedPacketCount() const { return m_droppedPacketCount; }
    quint64 activeSessionId() const { return m_activeSessionId; }
    bool isSessionAcksEnabled() const { return m_sessionAcksEnabled; }

private slots:
    void onNewConnection();

private:
    struct PeerState
    {
        QTcpSocket *socket = nullptr;
        Protocol::RdtpStreamParser parser;
        bool ackCapable = false;
        bool isChild = false;
        bool isAdmin = false;
    };

    struct QueuedDelivery
    {
        qint64 deliverAtMs = 0;
        quint64 sessionGeneration = 0;
        QByteArray rawPacket;
        bool isAck = false;
        quint32 ackFrameId = 0;
    };

    void onPeerReadyRead(QTcpSocket *socket);
    void onPeerDisconnected(QTcpSocket *socket);
    void processPeerMessages(PeerState &peer, const QByteArray &bytes);
    void handleMessage(PeerState &peer, const Protocol::ProtocolHeader &header, const QByteArray &payload);
    void handleRegisterHost(PeerState &peer, const Protocol::ProtocolHeader &header, const QByteArray &payload);
    void handleConnectRequest(PeerState &peer, const Protocol::ProtocolHeader &header, const QByteArray &payload);
    void handleSessionAccept(PeerState &peer, const Protocol::ProtocolHeader &header, const QByteArray &payload);
    void handleSessionReject(PeerState &peer, const Protocol::ProtocolHeader &header, const QByteArray &payload);
    void handleDisconnect(PeerState &peer, const Protocol::ProtocolHeader &header, const QByteArray &payload);
    void handleScreenAck(PeerState &peer, const Protocol::ProtocolHeader &header, const QByteArray &payload);
    void handleForwarding(PeerState &peer, const Protocol::ProtocolHeader &header, const QByteArray &payload);

    void enqueueChildToAdmin(const QByteArray &rawPacket);
    void enqueueAdminToChild(const QByteArray &rawPacket, bool isAck, quint32 ackFrameId);
    void flushChildToAdminQueue();
    void flushAdminToChildQueue();
    void flushDelayedAckQueue();
    void flushToChildQueue(QQueue<QueuedDelivery> &queue, QTimer &timer);
    void scheduleQueueTimer(QTimer &timer, const QQueue<QueuedDelivery> &queue);
    void applyReadBufferLimits();
    void drainChildSocketIfAllowed();

    void resetActiveSession(bool closeChild, bool closeAdmin);
    void recordViolation(const QString &reason);
    static QByteArray serializeMessage(Protocol::MessageType type,
                                       quint64 sessionId,
                                       const QByteArray &payload = {},
                                       quint32 sequenceNumber = 0,
                                       quint16 flags = 0);
    static bool parseAuthPayload(const QByteArray &payload, bool expectTarget, QString &outToken, QString &outTarget);
    static bool isValidInputMessage(Protocol::MessageType type, const QByteArray &payload);

    QTcpServer m_server;
    NetworkProfile m_profile;
    QElapsedTimer m_clock;
    std::mt19937 m_rng;

    QHash<QTcpSocket *, PeerState> m_peers;
    QTcpSocket *m_childSocket = nullptr;
    QTcpSocket *m_adminSocket = nullptr;
    quint64 m_nextSessionId = 1;
    quint64 m_pendingSessionId = 0;
    quint64 m_activeSessionId = 0;
    quint64 m_sessionGeneration = 0;
    bool m_sessionAcksEnabled = false;
    bool m_reservedScreenActive = false;

    QQueue<QueuedDelivery> m_childToAdminQueue;
    QQueue<QueuedDelivery> m_adminToChildQueue;
    QQueue<QueuedDelivery> m_delayedAckQueue;
    qint64 m_childToAdminQueuedBytes = 0;
    double m_childToAdminWireFreeAtMs = 0.0;
    qint64 m_lastChildToAdminDeliverMs = 0;
    qint64 m_lastAdminToChildDeliverMs = 0;
    qint64 m_lastAckDeliverMs = 0;

    QTimer m_childToAdminTimer;
    QTimer m_adminToChildTimer;
    QTimer m_delayedAckTimer;
    QList<QTimer *> m_scheduledTimers;

    QList<WirePacketRecord> m_recordedPackets;
    QStringList m_protocolViolations;
    int m_overBudgetPacketCount = 0;
    int m_droppedPacketCount = 0;
    QSet<quint32> m_inFlightFrameIds;
    int m_maxInFlightScreenFrames = 0;
};

struct E2eSessionPair
{
    std::unique_ptr<SimulatedRdtpRelay> relay;
    std::unique_ptr<ChildSessionController> child;
    std::unique_ptr<AdminSessionController> admin;
    std::unique_ptr<ScreenStreamSender> sender;

    QString previousToken;
    bool hasSavedToken = false;

    bool hasSavedTlsEnv = false;
    bool previousTlsSet = false;
    QByteArray previousTlsValue;
    bool previousCaCertSet = false;
    QByteArray previousCaCertValue;

    E2eSessionPair();
    ~E2eSessionPair();

    // Creates relay + controllers (+ optional sender) without starting session yet.
    bool initialize(const NetworkProfile &profile = {},
                    const QString &token = QStringLiteral("e2e-test-token"),
                    bool createSender = true);

    // Calls initialize() if needed, starts child, waits for relayRegistered, requests admin session, waits for both ends active.
    bool connectAndStartSession(const QString &agentUuid = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"),
                                const NetworkProfile &profile = {},
                                int timeoutMs = 5000,
                                bool createSender = true);

    void cleanup();
};

} // namespace E2eHarness
