#include "E2eRelayHarness.h"

#include "Network/Http/ApiClient.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include "Network/Protocol/ScreenFramePacketizer.h"
#include "Network/Relay/AdminSessionController.h"
#include "Network/Relay/ChildSessionController.h"
#include "Network/Relay/StaticRelayEndpointProvider.h"
#include "Streaming/ScreenStreamSender.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QUuid>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <limits>

namespace E2eHarness {

namespace {

bool isInputMessageType(Protocol::MessageType type)
{
    using T = Protocol::MessageType;
    return type == T::MOUSE_MOVE || type == T::MOUSE_BUTTON_DOWN || type == T::MOUSE_BUTTON_UP
        || type == T::MOUSE_WHEEL || type == T::KEY_PRESS || type == T::KEY_RELEASE;
}

bool isControlForwardType(Protocol::MessageType type)
{
    using T = Protocol::MessageType;
    return type == T::HEARTBEAT || type == T::PING || type == T::PONG || type == T::TELEMETRY;
}

} // namespace

SimulatedRdtpRelay::SimulatedRdtpRelay(QObject *parent)
    : QObject(parent)
    , m_rng(0x52445450u)
{
    m_childToAdminTimer.setSingleShot(true);
    m_childToAdminTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_childToAdminTimer, &QTimer::timeout, this, &SimulatedRdtpRelay::flushChildToAdminQueue);

    m_adminToChildTimer.setSingleShot(true);
    m_adminToChildTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_adminToChildTimer, &QTimer::timeout, this, &SimulatedRdtpRelay::flushAdminToChildQueue);

    m_delayedAckTimer.setSingleShot(true);
    m_delayedAckTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_delayedAckTimer, &QTimer::timeout, this, &SimulatedRdtpRelay::flushDelayedAckQueue);

    connect(&m_server, &QTcpServer::newConnection, this, &SimulatedRdtpRelay::onNewConnection);
}

SimulatedRdtpRelay::~SimulatedRdtpRelay()
{
    stop();
}

bool SimulatedRdtpRelay::start(quint16 port)
{
    stop();
    m_rng.seed(0x52445450u);
    m_nextSessionId = 1;
    m_pendingSessionId = 0;
    m_activeSessionId = 0;
    m_sessionGeneration = 0;
    m_sessionAcksEnabled = false;
    m_reservedScreenActive = false;
    m_childToAdminQueuedBytes = 0;
    m_childToAdminWireFreeAtMs = 0.0;
    m_lastChildToAdminDeliverMs = 0;
    m_lastAdminToChildDeliverMs = 0;
    m_lastAckDeliverMs = 0;
    m_recordedPackets.clear();
    m_protocolViolations.clear();
    m_overBudgetPacketCount = 0;
    m_droppedPacketCount = 0;
    m_inFlightFrameIds.clear();
    m_maxInFlightScreenFrames = 0;
    m_clock.restart();

    return m_server.listen(QHostAddress::LocalHost, port);
}

void SimulatedRdtpRelay::stop()
{
    for (QTimer *timer : m_scheduledTimers) {
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
    }
    m_scheduledTimers.clear();

    m_childToAdminTimer.stop();
    m_adminToChildTimer.stop();
    m_delayedAckTimer.stop();
    m_childToAdminQueue.clear();
    m_adminToChildQueue.clear();
    m_delayedAckQueue.clear();
    m_childToAdminQueuedBytes = 0;

    const auto sockets = m_peers.keys();
    m_peers.clear();
    m_childSocket = nullptr;
    m_adminSocket = nullptr;
    for (QTcpSocket *socket : sockets) {
        if (socket) {
            socket->disconnect(this);
            socket->abort();
            socket->deleteLater();
        }
    }

    m_pendingSessionId = 0;
    m_activeSessionId = 0;
    m_sessionAcksEnabled = false;
    m_reservedScreenActive = false;
    m_inFlightFrameIds.clear();

    if (m_server.isListening())
        m_server.close();
}

quint16 SimulatedRdtpRelay::port() const
{
    return m_server.serverPort();
}

void SimulatedRdtpRelay::setNetworkProfile(const NetworkProfile &profile)
{
    const NetworkProfile oldProfile = m_profile;
    m_profile = profile;

    if (oldProfile.simulateUnwritableAdmin && !m_profile.simulateUnwritableAdmin)
        m_reservedScreenActive = false;

    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;

    const bool bwBecameUnlimited = (m_profile.bandwidthBps <= 0.0 && oldProfile.bandwidthBps > 0.0);
    const bool bwIncreased = (m_profile.bandwidthBps > 0.0 && oldProfile.bandwidthBps > 0.0
                              && m_profile.bandwidthBps > oldProfile.bandwidthBps);
    const int oldChildOneWay = qMax(0, oldProfile.baseRttMs / 2);
    const int newChildOneWay = qMax(0, m_profile.baseRttMs / 2);
    const bool childOneWayDecreased = (newChildOneWay < oldChildOneWay);
    const bool jitterDecreased = (m_profile.jitterMs < oldProfile.jitterMs);

    if (bwBecameUnlimited || bwIncreased || childOneWayDecreased || jitterDecreased) {
        if (bwBecameUnlimited || bwIncreased || m_profile.bandwidthBps <= 0.0) {
            double wireFreeAt = static_cast<double>(now);
            qint64 lastDeliverAt = now;
            for (QueuedDelivery &item : m_childToAdminQueue) {
                const double txMs = (m_profile.bandwidthBps > 0.0)
                    ? (static_cast<double>(item.rawPacket.size()) * 1000.0 / m_profile.bandwidthBps)
                    : 0.0;
                wireFreeAt += txMs;
                const int jitter = (m_profile.jitterMs > 0)
                    ? std::uniform_int_distribution<int>(0, m_profile.jitterMs)(m_rng)
                    : 0;
                const qint64 recomputedDeliverAt =
                    static_cast<qint64>(std::llround(wireFreeAt)) + newChildOneWay + jitter;
                const qint64 deliverAt = qMax(lastDeliverAt, qMin(item.deliverAtMs, recomputedDeliverAt));
                item.deliverAtMs = deliverAt;
                lastDeliverAt = deliverAt;
            }
            m_childToAdminWireFreeAtMs = m_childToAdminQueue.isEmpty()
                ? static_cast<double>(now)
                : std::min(m_childToAdminWireFreeAtMs, wireFreeAt);
            m_lastChildToAdminDeliverMs = lastDeliverAt;
        } else {
            const qint64 rttReduction = qMax(0, oldChildOneWay - newChildOneWay);
            qint64 lastDeliverAt = now;
            for (QueuedDelivery &item : m_childToAdminQueue) {
                const int jitter = (m_profile.jitterMs > 0)
                    ? std::uniform_int_distribution<int>(0, m_profile.jitterMs)(m_rng)
                    : 0;
                const qint64 reduced = qMax(now + newChildOneWay + jitter, item.deliverAtMs - rttReduction);
                const qint64 deliverAt = qMax(lastDeliverAt, qMin(item.deliverAtMs, reduced));
                item.deliverAtMs = deliverAt;
                lastDeliverAt = deliverAt;
            }
            m_lastChildToAdminDeliverMs = m_childToAdminQueue.isEmpty()
                ? qMin(m_lastChildToAdminDeliverMs, now)
                : lastDeliverAt;
        }
        scheduleQueueTimer(m_childToAdminTimer, m_childToAdminQueue);
    }

    const int oldAdminOneWay = qMax(0, oldProfile.baseRttMs - (oldProfile.baseRttMs / 2));
    const int newAdminOneWay = qMax(0, m_profile.baseRttMs - (m_profile.baseRttMs / 2));
    const bool adminOneWayDecreased = (newAdminOneWay < oldAdminOneWay);

    if (adminOneWayDecreased || jitterDecreased) {
        qint64 lastDeliverAt = now;
        for (QueuedDelivery &item : m_adminToChildQueue) {
            const int jitter = (m_profile.jitterMs > 0)
                ? std::uniform_int_distribution<int>(0, m_profile.jitterMs)(m_rng)
                : 0;
            const qint64 recomputedDeliverAt = now + newAdminOneWay + jitter;
            const qint64 deliverAt = qMax(lastDeliverAt, qMin(item.deliverAtMs, recomputedDeliverAt));
            item.deliverAtMs = deliverAt;
            lastDeliverAt = deliverAt;
        }
        m_lastAdminToChildDeliverMs = m_adminToChildQueue.isEmpty()
            ? qMin(m_lastAdminToChildDeliverMs, now)
            : lastDeliverAt;
        scheduleQueueTimer(m_adminToChildTimer, m_adminToChildQueue);
    }

    const int oldAckDelay = qMax(0, oldProfile.ackDelayMs);
    const int newAckDelay = qMax(0, m_profile.ackDelayMs);
    const bool ackDelayDecreased = (newAckDelay < oldAckDelay);

    if (ackDelayDecreased || adminOneWayDecreased || jitterDecreased) {
        qint64 lastDeliverAt = now;
        for (QueuedDelivery &item : m_delayedAckQueue) {
            const int jitter = (m_profile.jitterMs > 0)
                ? std::uniform_int_distribution<int>(0, m_profile.jitterMs)(m_rng)
                : 0;
            const qint64 recomputedDeliverAt = now + newAdminOneWay + newAckDelay + jitter;
            const qint64 deliverAt = qMax(lastDeliverAt, qMin(item.deliverAtMs, recomputedDeliverAt));
            item.deliverAtMs = deliverAt;
            lastDeliverAt = deliverAt;
        }
        m_lastAckDeliverMs = m_delayedAckQueue.isEmpty()
            ? qMin(m_lastAckDeliverMs, now)
            : lastDeliverAt;
        scheduleQueueTimer(m_delayedAckTimer, m_delayedAckQueue);
    }

    if (m_profile.readBufferLimitBytes != oldProfile.readBufferLimitBytes) {
        applyReadBufferLimits();
        drainChildSocketIfAllowed();
    }
}

void SimulatedRdtpRelay::scheduleProfileChange(int delayMs, const NetworkProfile &profile)
{
    if (delayMs <= 0) {
        setNetworkProfile(profile);
        return;
    }

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setTimerType(Qt::PreciseTimer);
    m_scheduledTimers.append(timer);

    connect(timer, &QTimer::timeout, this, [this, timer, profile]() {
        m_scheduledTimers.removeOne(timer);
        timer->deleteLater();
        setNetworkProfile(profile);
    });
    timer->start(delayMs);
}

void SimulatedRdtpRelay::clearRecordedPackets()
{
    m_recordedPackets.clear();
}

void SimulatedRdtpRelay::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        if (!socket)
            continue;

        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

        PeerState state;
        state.socket = socket;
        m_peers.insert(socket, state);

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            onPeerReadyRead(socket);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            onPeerDisconnected(socket);
        });
    }
}

void SimulatedRdtpRelay::onPeerReadyRead(QTcpSocket *socket)
{
    if (!socket || !m_peers.contains(socket))
        return;

    if (socket == m_childSocket && m_profile.readBufferLimitBytes > 0) {
        if (m_childToAdminQueuedBytes >= m_profile.readBufferLimitBytes)
            return;
    }

    const QByteArray bytes = socket->readAll();
    if (bytes.isEmpty())
        return;

    auto it = m_peers.find(socket);
    if (it == m_peers.end())
        return;

    processPeerMessages(*it, bytes);
}

void SimulatedRdtpRelay::onPeerDisconnected(QTcpSocket *socket)
{
    if (!socket)
        return;

    m_peers.remove(socket);
    socket->disconnect(this);
    socket->deleteLater();

    if (socket == m_childSocket) {
        m_childSocket = nullptr;
        resetActiveSession(false, true);
    } else if (socket == m_adminSocket) {
        m_adminSocket = nullptr;
        resetActiveSession(true, false);
    }
}

void SimulatedRdtpRelay::processPeerMessages(PeerState &peer, const QByteArray &bytes)
{
    const auto result = peer.parser.feed(bytes);
    if (result.error != Protocol::RdtpStreamParser::Error::None) {
        recordViolation(QStringLiteral("RdtpStreamParser error %1")
                            .arg(static_cast<int>(result.error)));
        if (peer.socket)
            peer.socket->close();
        return;
    }

    QTcpSocket *socket = peer.socket;
    for (const auto &message : result.messages) {
        if (!m_peers.contains(socket))
            return;
        handleMessage(m_peers[socket], message.header, message.payload);
    }
}

void SimulatedRdtpRelay::handleMessage(PeerState &peer,
                                       const Protocol::ProtocolHeader &header,
                                       const QByteArray &payload)
{
    using T = Protocol::MessageType;
    switch (header.type) {
    case T::REGISTER_HOST:
        handleRegisterHost(peer, header, payload);
        break;
    case T::CONNECT_REQUEST:
        handleConnectRequest(peer, header, payload);
        break;
    case T::SESSION_ACCEPT:
        handleSessionAccept(peer, header, payload);
        break;
    case T::SESSION_REJECT:
        handleSessionReject(peer, header, payload);
        break;
    case T::DISCONNECT:
        handleDisconnect(peer, header, payload);
        break;
    case T::SCREEN_FRAME_ACK:
        handleScreenAck(peer, header, payload);
        break;
    case T::SCREEN_FRAME:
    case T::MOUSE_MOVE:
    case T::MOUSE_BUTTON_DOWN:
    case T::MOUSE_BUTTON_UP:
    case T::MOUSE_WHEEL:
    case T::KEY_PRESS:
    case T::KEY_RELEASE:
    case T::HEARTBEAT:
    case T::PING:
    case T::PONG:
    case T::TELEMETRY:
        handleForwarding(peer, header, payload);
        break;
    default:
        recordViolation(QStringLiteral("Unexpected message type 0x%1")
                            .arg(static_cast<int>(header.type), 2, 16, QLatin1Char('0')));
        break;
    }
}

void SimulatedRdtpRelay::handleRegisterHost(PeerState &peer,
                                            const Protocol::ProtocolHeader &header,
                                            const QByteArray &payload)
{
    WirePacketRecord rec;
    rec.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
    rec.fromChild = true;
    rec.header = header;
    rec.wireSize = Protocol::HEADER_SIZE + payload.size();
    m_recordedPackets.append(rec);

    QString token;
    QString target;
    const bool validHeader = (header.flags == 0 && header.sessionId == 0 && header.sequenceNumber == 0);
    const bool validPayload = parseAuthPayload(payload, false, token, target);

    if (!validHeader || !validPayload) {
        recordViolation(QStringLiteral("Invalid REGISTER_HOST header or auth payload"));
        if (peer.socket)
            peer.socket->write(serializeMessage(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\x00')));
        return;
    }

    if (m_childSocket != nullptr && m_childSocket != peer.socket) {
        recordViolation(QStringLiteral("Duplicate REGISTER_HOST while another child is connected"));
        if (peer.socket)
            peer.socket->write(serializeMessage(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\x00')));
        return;
    }

    peer.isChild = true;
    m_childSocket = peer.socket;
    applyReadBufferLimits();

    if (peer.socket)
        peer.socket->write(serializeMessage(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\x01')));
}

void SimulatedRdtpRelay::handleConnectRequest(PeerState &peer,
                                              const Protocol::ProtocolHeader &header,
                                              const QByteArray &payload)
{
    WirePacketRecord rec;
    rec.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
    rec.fromChild = false;
    rec.header = header;
    rec.wireSize = Protocol::HEADER_SIZE + payload.size();
    m_recordedPackets.append(rec);

    QString token;
    QString target;
    const bool validHeader = (header.flags == 0 && header.sessionId == 0 && header.sequenceNumber == 0);
    const bool validPayload = parseAuthPayload(payload, true, token, target);

    if (!validHeader || !validPayload) {
        recordViolation(QStringLiteral("Invalid CONNECT_REQUEST header or auth payload"));
        if (peer.socket)
            peer.socket->write(serializeMessage(Protocol::MessageType::CONNECT_RESULT, 0, QByteArray(1, '\x00')));
        return;
    }

    const QUuid uuid = QUuid::fromString(target);
    if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != target) {
        recordViolation(QStringLiteral("CONNECT_REQUEST target is not a canonical lowercase UUID: %1").arg(target));
        if (peer.socket)
            peer.socket->write(serializeMessage(Protocol::MessageType::CONNECT_RESULT, 0, QByteArray(1, '\x00')));
        return;
    }

    if (!m_childSocket || m_childSocket == peer.socket || !m_peers.contains(m_childSocket)
        || m_activeSessionId != 0 || m_pendingSessionId != 0) {
        if (peer.socket)
            peer.socket->write(serializeMessage(Protocol::MessageType::CONNECT_RESULT, 0, QByteArray(1, '\x00')));
        return;
    }

    m_pendingSessionId = m_nextSessionId++;
    peer.isAdmin = true;
    m_adminSocket = peer.socket;

    m_sessionAcksEnabled = m_profile.ackCapabilityEnabled && peer.ackCapable && m_peers[m_childSocket].ackCapable;

    // Critical handshake order for ChildSessionController:
    // SCREEN_FRAME_ACK(flags=1, sessionId=S) MUST arrive before SESSION_REQUEST(S).
    if (m_sessionAcksEnabled) {
        m_childSocket->write(serializeMessage(Protocol::MessageType::SCREEN_FRAME_ACK,
                                              m_pendingSessionId,
                                              {},
                                              0,
                                              Protocol::SCREEN_FRAME_ACK_CAPABILITY_FLAG));
    }
    m_childSocket->write(serializeMessage(Protocol::MessageType::SESSION_REQUEST, m_pendingSessionId));
}

void SimulatedRdtpRelay::handleSessionAccept(PeerState &peer,
                                             const Protocol::ProtocolHeader &header,
                                             const QByteArray &payload)
{
    WirePacketRecord rec;
    rec.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
    rec.fromChild = true;
    rec.header = header;
    rec.wireSize = Protocol::HEADER_SIZE + payload.size();
    m_recordedPackets.append(rec);

    if (peer.socket != m_childSocket || m_pendingSessionId == 0 || header.sessionId != m_pendingSessionId
        || header.flags != 0 || header.sequenceNumber != 0 || !payload.isEmpty()) {
        recordViolation(QStringLiteral("Invalid SESSION_ACCEPT"));
        return;
    }

    m_activeSessionId = m_pendingSessionId;
    m_pendingSessionId = 0;
    ++m_sessionGeneration;
    m_inFlightFrameIds.clear();
    m_reservedScreenActive = false;

    if (m_adminSocket && m_adminSocket->state() == QAbstractSocket::ConnectedState) {
        // Critical handshake order for AdminSessionController:
        // CONNECT_RESULT(S, {1}) MUST arrive before SCREEN_FRAME_ACK(flags=1, sessionId=S).
        m_adminSocket->write(serializeMessage(Protocol::MessageType::CONNECT_RESULT,
                                              m_activeSessionId,
                                              QByteArray(1, '\x01')));
        if (m_sessionAcksEnabled) {
            m_adminSocket->write(serializeMessage(Protocol::MessageType::SCREEN_FRAME_ACK,
                                                  m_activeSessionId,
                                                  {},
                                                  0,
                                                  Protocol::SCREEN_FRAME_ACK_CAPABILITY_FLAG));
        }
    }
}

void SimulatedRdtpRelay::handleSessionReject(PeerState &peer,
                                             const Protocol::ProtocolHeader &header,
                                             const QByteArray &payload)
{
    WirePacketRecord rec;
    rec.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
    rec.fromChild = true;
    rec.header = header;
    rec.wireSize = Protocol::HEADER_SIZE + payload.size();
    m_recordedPackets.append(rec);

    if (peer.socket != m_childSocket || m_pendingSessionId == 0 || header.sessionId != m_pendingSessionId
        || header.flags != 0 || header.sequenceNumber != 0 || !payload.isEmpty()) {
        recordViolation(QStringLiteral("Invalid SESSION_REJECT"));
        return;
    }

    m_pendingSessionId = 0;
    m_sessionAcksEnabled = false;
    if (m_adminSocket && m_adminSocket->state() == QAbstractSocket::ConnectedState) {
        m_adminSocket->write(serializeMessage(Protocol::MessageType::CONNECT_RESULT,
                                              0,
                                              QByteArray(1, '\x00')));
    }
}

void SimulatedRdtpRelay::handleDisconnect(PeerState &peer,
                                          const Protocol::ProtocolHeader &header,
                                          const QByteArray &payload)
{
    WirePacketRecord rec;
    rec.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
    rec.fromChild = (peer.socket == m_childSocket);
    rec.header = header;
    rec.wireSize = Protocol::HEADER_SIZE + payload.size();
    m_recordedPackets.append(rec);

    if (header.flags != 0 || header.sequenceNumber != 0 || !payload.isEmpty()) {
        recordViolation(QStringLiteral("Invalid DISCONNECT header/payload"));
        return;
    }

    if (m_activeSessionId == 0 || header.sessionId != m_activeSessionId)
        return;

    QTcpSocket *target = (peer.socket == m_adminSocket) ? m_childSocket : m_adminSocket;
    if (target && target->state() == QAbstractSocket::ConnectedState) {
        target->write(serializeMessage(Protocol::MessageType::DISCONNECT, m_activeSessionId));
    }
    resetActiveSession(false, false);
}

void SimulatedRdtpRelay::handleScreenAck(PeerState &peer,
                                         const Protocol::ProtocolHeader &header,
                                         const QByteArray &payload)
{
    if (header.payloadLength != 0 || !payload.isEmpty()) {
        recordViolation(QStringLiteral("SCREEN_FRAME_ACK must have empty payload"));
        return;
    }

    WirePacketRecord rec;
    rec.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
    rec.fromChild = (peer.socket == m_childSocket);
    rec.header = header;
    rec.wireSize = Protocol::HEADER_SIZE;
    if (header.flags == 0)
        rec.frameId = header.sequenceNumber;
    m_recordedPackets.append(rec);

    if (header.flags == Protocol::SCREEN_FRAME_ACK_CAPABILITY_FLAG
        && header.sessionId == 0 && header.sequenceNumber == 0
        && !peer.isChild && !peer.isAdmin) {
        peer.ackCapable = true;
        return;
    }

    if (header.flags == 0 && peer.socket == m_adminSocket) {
        if (header.sessionId > 0 && header.sessionId < m_nextSessionId
            && header.sessionId != m_activeSessionId && header.sessionId != m_pendingSessionId) {
            return;
        }

        if (m_activeSessionId != 0 && header.sessionId == m_activeSessionId) {
            if (!m_sessionAcksEnabled)
                return;

            if (m_profile.dropAcks) {
                ++m_droppedPacketCount;
                return;
            }

            const QByteArray rawPacket = serializeMessage(Protocol::MessageType::SCREEN_FRAME_ACK,
                                                          header.sessionId,
                                                          {},
                                                          header.sequenceNumber,
                                                          0);
            enqueueAdminToChild(rawPacket, true, header.sequenceNumber);
            return;
        }
    }

    recordViolation(QStringLiteral("Unexpected SCREEN_FRAME_ACK flags=%1 sessionId=%2")
                        .arg(header.flags)
                        .arg(header.sessionId));
}

void SimulatedRdtpRelay::handleForwarding(PeerState &peer,
                                          const Protocol::ProtocolHeader &header,
                                          const QByteArray &payload)
{
    using T = Protocol::MessageType;

    if (header.type == T::SCREEN_FRAME) {
        if (peer.socket == m_childSocket
            && header.sessionId > 0 && header.sessionId < m_nextSessionId
            && header.sessionId != m_activeSessionId && header.sessionId != m_pendingSessionId) {
            return;
        }

        if (peer.socket != m_childSocket || m_activeSessionId == 0 || header.sessionId != m_activeSessionId) {
            recordViolation(QStringLiteral("SCREEN_FRAME outside active child session"));
            return;
        }

        const qsizetype wireSize = static_cast<qsizetype>(Protocol::HEADER_SIZE) + payload.size();
        WirePacketRecord rec;
        rec.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
        rec.fromChild = true;
        rec.header = header;
        rec.wireSize = wireSize;

        if (payload.size() < ScreenFramePacketizer::SCREEN_FRAME_METADATA_SIZE) {
            m_recordedPackets.append(rec);
            recordViolation(QStringLiteral("SCREEN_FRAME payload shorter than 16-byte metadata"));
            return;
        }

        const auto *meta = reinterpret_cast<const uchar *>(payload.constData());
        const quint32 frameId = qFromBigEndian<quint32>(meta);
        const quint32 chunkIndex = qFromBigEndian<quint32>(meta + 4);
        const quint32 chunkCount = qFromBigEndian<quint32>(meta + 8);
        const quint32 totalFrameSize = qFromBigEndian<quint32>(meta + 12);
        const qsizetype chunkBytes = payload.size() - ScreenFramePacketizer::SCREEN_FRAME_METADATA_SIZE;

        rec.frameId = frameId;
        rec.chunkIndex = chunkIndex;
        rec.chunkCount = chunkCount;
        rec.totalFrameSize = totalFrameSize;
        m_recordedPackets.append(rec);

        if (chunkCount == 0 || chunkIndex >= chunkCount || totalFrameSize == 0
            || chunkBytes <= 0 || static_cast<quint32>(chunkBytes) > totalFrameSize) {
            recordViolation(QStringLiteral("Invalid SCREEN_FRAME chunk metadata (frame=%1 idx=%2/%3 size=%4 chunk=%5)")
                                .arg(frameId)
                                .arg(chunkIndex)
                                .arg(chunkCount)
                                .arg(totalFrameSize)
                                .arg(chunkBytes));
            return;
        }

        if (wireSize > MAX_RESERVED_WIRE_BYTES)
            ++m_overBudgetPacketCount;

        if (m_profile.chunkDropIndices.contains(chunkIndex)) {
            ++m_droppedPacketCount;
            return;
        }

        if (m_profile.lossPercent > 0.0) {
            std::uniform_real_distribution<double> dist(0.0, 100.0);
            if (dist(m_rng) < m_profile.lossPercent) {
                ++m_droppedPacketCount;
                return;
            }
        }

        if (m_profile.simulateUnwritableAdmin) {
            if (!m_sessionAcksEnabled || wireSize > MAX_RESERVED_WIRE_BYTES || m_reservedScreenActive) {
                ++m_droppedPacketCount;
                return;
            }
            m_reservedScreenActive = true;
        }

        m_inFlightFrameIds.insert(frameId);
        if (m_inFlightFrameIds.size() > m_maxInFlightScreenFrames)
            m_maxInFlightScreenFrames = static_cast<int>(m_inFlightFrameIds.size());

        const QByteArray rawPacket = Protocol::ProtocolSerializer::serializeHeader(header) + payload;
        enqueueChildToAdmin(rawPacket);
        return;
    }

    if (isInputMessageType(header.type)) {
        if (peer.socket == m_adminSocket
            && header.sessionId > 0 && header.sessionId < m_nextSessionId
            && header.sessionId != m_activeSessionId && header.sessionId != m_pendingSessionId) {
            return;
        }

        if (peer.socket != m_adminSocket || m_activeSessionId == 0 || header.sessionId != m_activeSessionId) {
            recordViolation(QStringLiteral("Input message outside active admin session"));
            return;
        }

        WirePacketRecord rec;
        rec.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
        rec.fromChild = false;
        rec.header = header;
        rec.wireSize = static_cast<qsizetype>(Protocol::HEADER_SIZE) + payload.size();
        m_recordedPackets.append(rec);

        if (!isValidInputMessage(header.type, payload)) {
            recordViolation(QStringLiteral("Invalid input payload for type 0x%1 (size=%2)")
                                .arg(static_cast<int>(header.type), 2, 16, QLatin1Char('0'))
                                .arg(payload.size()));
            if (m_adminSocket)
                m_adminSocket->close();
            return;
        }

        const QByteArray rawPacket = Protocol::ProtocolSerializer::serializeHeader(header) + payload;
        enqueueAdminToChild(rawPacket, false, 0);
        return;
    }

    if (isControlForwardType(header.type)) {
        if (payload.size() > MAX_CONTROL_PAYLOAD_BYTES) {
            recordViolation(QStringLiteral("Control payload exceeds 8192 bytes: %1").arg(payload.size()));
            return;
        }

        WirePacketRecord rec;
        rec.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
        rec.fromChild = (peer.socket == m_childSocket);
        rec.header = header;
        rec.wireSize = static_cast<qsizetype>(Protocol::HEADER_SIZE) + payload.size();
        m_recordedPackets.append(rec);

        if (m_activeSessionId == 0 || header.sessionId != m_activeSessionId)
            return;

        const QByteArray rawPacket = Protocol::ProtocolSerializer::serializeHeader(header) + payload;
        if (peer.socket == m_childSocket)
            enqueueChildToAdmin(rawPacket);
        else if (peer.socket == m_adminSocket)
            enqueueAdminToChild(rawPacket, false, 0);
        return;
    }
}

void SimulatedRdtpRelay::enqueueChildToAdmin(const QByteArray &rawPacket)
{
    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;
    const double txMs = (m_profile.bandwidthBps > 0.0)
        ? (static_cast<double>(rawPacket.size()) * 1000.0 / m_profile.bandwidthBps)
        : 0.0;

    m_childToAdminWireFreeAtMs = std::max(static_cast<double>(now), m_childToAdminWireFreeAtMs) + txMs;

    const int oneWayBaseMs = qMax(0, m_profile.baseRttMs / 2);
    const int jitter = (m_profile.jitterMs > 0)
        ? std::uniform_int_distribution<int>(0, m_profile.jitterMs)(m_rng)
        : 0;
    const qint64 deliverAt = qMax(m_lastChildToAdminDeliverMs,
                                  static_cast<qint64>(std::llround(m_childToAdminWireFreeAtMs)) + oneWayBaseMs + jitter);
    m_lastChildToAdminDeliverMs = deliverAt;

    QueuedDelivery item;
    item.deliverAtMs = deliverAt;
    item.sessionGeneration = m_sessionGeneration;
    item.rawPacket = rawPacket;

    m_childToAdminQueue.enqueue(item);
    m_childToAdminQueuedBytes += rawPacket.size();
    scheduleQueueTimer(m_childToAdminTimer, m_childToAdminQueue);
}

void SimulatedRdtpRelay::enqueueAdminToChild(const QByteArray &rawPacket, bool isAck, quint32 ackFrameId)
{
    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;
    const int oneWayBaseMs = qMax(0, m_profile.baseRttMs - (m_profile.baseRttMs / 2));
    const int jitter = (m_profile.jitterMs > 0)
        ? std::uniform_int_distribution<int>(0, m_profile.jitterMs)(m_rng)
        : 0;

    QueuedDelivery item;
    item.sessionGeneration = m_sessionGeneration;
    item.rawPacket = rawPacket;
    item.isAck = isAck;
    item.ackFrameId = ackFrameId;

    if (isAck && m_profile.ackDelayMs > 0) {
        const qint64 deliverAt = qMax(m_lastAckDeliverMs,
                                      now + oneWayBaseMs + m_profile.ackDelayMs + jitter);
        m_lastAckDeliverMs = deliverAt;
        item.deliverAtMs = deliverAt;
        m_delayedAckQueue.enqueue(item);
        scheduleQueueTimer(m_delayedAckTimer, m_delayedAckQueue);
    } else {
        const qint64 deliverAt = qMax(m_lastAdminToChildDeliverMs,
                                      now + oneWayBaseMs + jitter);
        m_lastAdminToChildDeliverMs = deliverAt;
        item.deliverAtMs = deliverAt;
        m_adminToChildQueue.enqueue(item);
        scheduleQueueTimer(m_adminToChildTimer, m_adminToChildQueue);
    }
}

void SimulatedRdtpRelay::flushChildToAdminQueue()
{
    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;
    while (!m_childToAdminQueue.isEmpty() && m_childToAdminQueue.head().deliverAtMs <= now) {
        const QueuedDelivery item = m_childToAdminQueue.dequeue();
        m_childToAdminQueuedBytes = qMax<qint64>(0, m_childToAdminQueuedBytes - item.rawPacket.size());

        if (item.sessionGeneration == m_sessionGeneration && m_activeSessionId != 0
            && m_adminSocket && m_adminSocket->state() == QAbstractSocket::ConnectedState) {
            m_adminSocket->write(item.rawPacket);
        }
    }

    scheduleQueueTimer(m_childToAdminTimer, m_childToAdminQueue);
    drainChildSocketIfAllowed();
}

void SimulatedRdtpRelay::flushAdminToChildQueue()
{
    flushToChildQueue(m_adminToChildQueue, m_adminToChildTimer);
}

void SimulatedRdtpRelay::flushDelayedAckQueue()
{
    flushToChildQueue(m_delayedAckQueue, m_delayedAckTimer);
}

void SimulatedRdtpRelay::flushToChildQueue(QQueue<QueuedDelivery> &queue, QTimer &timer)
{
    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;
    while (!queue.isEmpty() && queue.head().deliverAtMs <= now) {
        const QueuedDelivery item = queue.dequeue();
        if (item.sessionGeneration == m_sessionGeneration && m_activeSessionId != 0
            && m_childSocket && m_childSocket->state() == QAbstractSocket::ConnectedState) {
            m_childSocket->write(item.rawPacket);
            if (item.isAck)
                m_inFlightFrameIds.remove(item.ackFrameId);
        }
    }
    scheduleQueueTimer(timer, queue);
}

void SimulatedRdtpRelay::scheduleQueueTimer(QTimer &timer, const QQueue<QueuedDelivery> &queue)
{
    if (queue.isEmpty()) {
        timer.stop();
        return;
    }
    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;
    const qint64 delayMs = qMax<qint64>(0, queue.head().deliverAtMs - now);
    timer.start(static_cast<int>(qMin<qint64>(delayMs, std::numeric_limits<int>::max())));
}

void SimulatedRdtpRelay::applyReadBufferLimits()
{
    if (!m_childSocket)
        return;
    const qint64 limit = qMax<qint64>(0, m_profile.readBufferLimitBytes);
    m_childSocket->setReadBufferSize(limit);
}

void SimulatedRdtpRelay::drainChildSocketIfAllowed()
{
    if (!m_childSocket || !m_peers.contains(m_childSocket))
        return;
    if (m_profile.readBufferLimitBytes > 0 && m_childToAdminQueuedBytes >= m_profile.readBufferLimitBytes)
        return;
    if (m_childSocket->bytesAvailable() > 0)
        onPeerReadyRead(m_childSocket);
}

void SimulatedRdtpRelay::resetActiveSession(bool closeChild, bool closeAdmin)
{
    m_pendingSessionId = 0;
    m_activeSessionId = 0;
    ++m_sessionGeneration;
    m_sessionAcksEnabled = false;
    m_reservedScreenActive = false;
    m_inFlightFrameIds.clear();

    m_childToAdminQueue.clear();
    m_adminToChildQueue.clear();
    m_delayedAckQueue.clear();
    m_childToAdminQueuedBytes = 0;
    m_childToAdminWireFreeAtMs = 0.0;
    m_lastChildToAdminDeliverMs = 0;
    m_lastAdminToChildDeliverMs = 0;
    m_lastAckDeliverMs = 0;
    m_childToAdminTimer.stop();
    m_adminToChildTimer.stop();
    m_delayedAckTimer.stop();

    if (closeChild && m_childSocket)
        m_childSocket->close();
    if (closeAdmin && m_adminSocket)
        m_adminSocket->close();
}

void SimulatedRdtpRelay::recordViolation(const QString &reason)
{
    m_protocolViolations.append(reason);
}

QByteArray SimulatedRdtpRelay::serializeMessage(Protocol::MessageType type,
                                                quint64 sessionId,
                                                const QByteArray &payload,
                                                quint32 sequenceNumber,
                                                quint16 flags)
{
    Protocol::ProtocolHeader header(type);
    header.sessionId = sessionId;
    header.sequenceNumber = sequenceNumber;
    header.flags = flags;
    header.payloadLength = static_cast<uint32_t>(payload.size());
    return Protocol::ProtocolSerializer::serializeHeader(header) + payload;
}

bool SimulatedRdtpRelay::parseAuthPayload(const QByteArray &payload,
                                          bool expectTarget,
                                          QString &outToken,
                                          QString &outTarget)
{
    if (payload.size() > 4200 || payload.size() < 2)
        return false;

    const auto *data = reinterpret_cast<const uchar *>(payload.constData());
    const quint16 tokenLen = qFromBigEndian<quint16>(data);
    if (tokenLen == 0 || tokenLen > 4096)
        return false;

    qsizetype offset = 2 + tokenLen;
    if (payload.size() < offset)
        return false;

    outToken = QString::fromUtf8(payload.constData() + 2, tokenLen);

    if (!expectTarget) {
        outTarget.clear();
        return payload.size() == offset;
    }

    if (payload.size() < offset + 2)
        return false;

    const quint16 targetLen = qFromBigEndian<quint16>(data + offset);
    offset += 2;
    if (targetLen == 0 || targetLen > 36 || payload.size() != offset + targetLen)
        return false;

    outTarget = QString::fromUtf8(payload.constData() + offset, targetLen);
    return true;
}

bool SimulatedRdtpRelay::isValidInputMessage(Protocol::MessageType type, const QByteArray &payload)
{
    using T = Protocol::MessageType;
    if (payload.size() > MAX_CONTROL_PAYLOAD_BYTES)
        return false;

    qsizetype baseSize = 0;
    bool allowFrameGeometry = false;
    switch (type) {
    case T::MOUSE_MOVE:
        baseSize = 8;
        allowFrameGeometry = true;
        break;
    case T::MOUSE_BUTTON_DOWN:
    case T::MOUSE_BUTTON_UP:
        baseSize = 9;
        allowFrameGeometry = true;
        break;
    case T::MOUSE_WHEEL:
        baseSize = 12;
        allowFrameGeometry = true;
        break;
    case T::KEY_PRESS:
    case T::KEY_RELEASE:
        baseSize = 4;
        allowFrameGeometry = false;
        break;
    default:
        return false;
    }

    if (payload.size() == baseSize)
        return true;

    if (!allowFrameGeometry || payload.size() != baseSize + 8)
        return false;

    const QSize frameSize = RemoteInputPacket::referenceFrame(payload, baseSize, {});
    return frameSize.isValid();
}

E2eSessionPair::E2eSessionPair() = default;

E2eSessionPair::~E2eSessionPair()
{
    cleanup();
}

bool E2eSessionPair::initialize(const NetworkProfile &profile,
                                const QString &token,
                                bool createSender)
{
    cleanup();

    if (!hasSavedTlsEnv) {
        previousTlsSet = qEnvironmentVariableIsSet("REMOTE_RELAY_TLS");
        previousTlsValue = qgetenv("REMOTE_RELAY_TLS");
        previousCaCertSet = qEnvironmentVariableIsSet("REMOTE_RELAY_CA_CERT");
        previousCaCertValue = qgetenv("REMOTE_RELAY_CA_CERT");
        hasSavedTlsEnv = true;
    }
    qunsetenv("REMOTE_RELAY_TLS");
    qunsetenv("REMOTE_RELAY_CA_CERT");

    previousToken = ApiClient::instance().getToken();
    hasSavedToken = true;
    ApiClient::instance().setToken(token);

    relay = std::make_unique<SimulatedRdtpRelay>();
    relay->setNetworkProfile(profile);
    if (!relay->start(0))
        return false;

    const quint16 relayPort = relay->port();
    auto *childProvider = new StaticRelayEndpointProvider(QStringLiteral("127.0.0.1"), relayPort);
    child = std::make_unique<ChildSessionController>(childProvider);
    childProvider->setParent(child.get());

    if (createSender)
        sender = std::make_unique<ScreenStreamSender>(child.get());

    auto *adminProvider = new StaticRelayEndpointProvider(QStringLiteral("127.0.0.1"), relayPort, nullptr);
    admin = std::make_unique<AdminSessionController>(adminProvider, nullptr);

    return true;
}

bool E2eSessionPair::connectAndStartSession(const QString &agentUuid,
                                            const NetworkProfile &profile,
                                            int timeoutMs,
                                            bool createSender)
{
    if (!relay && !initialize(profile, QStringLiteral("e2e-test-token"), createSender))
        return false;

    const QUuid parsedUuid = QUuid::fromString(agentUuid);
    if (parsedUuid.isNull())
        return false;
    const QString normalizedUuid = parsedUuid.toString(QUuid::WithoutBraces);

    if (!child->relayReady()) {
        QSignalSpy registeredSpy(child.get(), &ChildSessionController::relayRegistered);
        child->start();

        QElapsedTimer timer;
        timer.start();
        while (!child->relayReady() && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        if (!child->relayReady() && registeredSpy.count() == 0)
            return false;
    }

    QSignalSpy childStartedSpy(child.get(), &ChildSessionController::sessionStarted);
    QSignalSpy adminEstablishedSpy(admin.get(), &AdminSessionController::sessionEstablished);
    QSignalSpy adminFailedSpy(admin.get(), &AdminSessionController::requestFailed);

    admin->requestSession(normalizedUuid);

    QElapsedTimer timer;
    timer.start();
    while ((child->activeSessionId() == 0 || adminEstablishedSpy.count() == 0)
           && adminFailedSpy.count() == 0
           && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    return child->activeSessionId() != 0
        && childStartedSpy.count() >= 1
        && adminEstablishedSpy.count() >= 1;
}

void E2eSessionPair::cleanup()
{
    if (child)
        child->stop();
    if (admin)
        admin->disconnectSession();

    // ScreenStreamSender holds a raw ChildSessionController * pointer, so destroy sender first.
    sender.reset();
    child.reset();
    admin.reset();

    if (relay)
        relay->stop();
    relay.reset();

    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    if (hasSavedToken) {
        ApiClient::instance().setToken(previousToken);
        previousToken.clear();
        hasSavedToken = false;
    }

    if (hasSavedTlsEnv) {
        if (previousTlsSet)
            qputenv("REMOTE_RELAY_TLS", previousTlsValue);
        else
            qunsetenv("REMOTE_RELAY_TLS");

        if (previousCaCertSet)
            qputenv("REMOTE_RELAY_CA_CERT", previousCaCertValue);
        else
            qunsetenv("REMOTE_RELAY_CA_CERT");

        previousTlsValue.clear();
        previousCaCertValue.clear();
        hasSavedTlsEnv = false;
    }
}

} // namespace E2eHarness
