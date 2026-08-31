#include "ScreenStreamSender.h"

#include "ScreenCapture.h"
#include "ScreenEncoder.h"
#include "../../Network/relayclient.h"
#include "../../Network/protocol/ScreenFramePacketizer.h"
#include "../../Network/protocol/protocolserializer.h"

#include <QDebug>

// Relay host for local / LAN testing.
static const QString RELAY_HOST = QStringLiteral("localhost");

ScreenStreamSender::ScreenStreamSender(const QString &childUsername, QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_relayClient(new RelayClient(this))
    , m_frameId(1)
    , m_childUsername(childUsername)
    , m_registered(false)
    , m_currentSessionId(0)
{
    m_timer->setInterval(STREAM_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &ScreenStreamSender::onTick);
    connect(m_relayClient, &RelayClient::connected,
            this, &ScreenStreamSender::onRelayConnected);
    connect(m_relayClient, &RelayClient::disconnected,
            this, &ScreenStreamSender::onRelayDisconnected);
    connect(m_relayClient, &RelayClient::bytesReceived,
            this, &ScreenStreamSender::onRelayBytesReceived);
}

ScreenStreamSender::~ScreenStreamSender()
{
    stop();
}

// ---------------------------------------------------------------------------
// start()
// Connect to the Relay Proxy once then start the repeating capture timer.
// ---------------------------------------------------------------------------

void ScreenStreamSender::start()
{
    m_relayClient->ConnectToServer(RELAY_HOST, RELAY_PORT);
    m_timer->start();
    qDebug() << "[ScreenStreamSender] Started."
             << "host=" << RELAY_HOST
             << "port=" << RELAY_PORT
             << "interval=" << STREAM_INTERVAL_MS << "ms";
}

void ScreenStreamSender::stop()
{
    if (m_timer->isActive()) {
        m_timer->stop();
        qDebug() << "[ScreenStreamSender] Stopped.";
    }
}

void ScreenStreamSender::onRelayConnected()
{
    m_registered = false;
    m_streamParser = Protocol::RdtpStreamParser{};
    sendRegisterHost();
}

void ScreenStreamSender::onRelayDisconnected()
{
    m_registered = false;
    m_currentSessionId = 0;
    m_streamParser = Protocol::RdtpStreamParser{};
}

void ScreenStreamSender::sendRegisterHost()
{
    const QByteArray usernameBytes = m_childUsername.toUtf8();
    if (usernameBytes.isEmpty() || usernameBytes.size() > 200) {
        qWarning() << "[ScreenStreamSender] Cannot register: username UTF-8 length must be 1..200 bytes.";
        return;
    }

    QByteArray payload;
    payload.reserve(2 + usernameBytes.size());
    const quint16 usernameLength = static_cast<quint16>(usernameBytes.size());
    payload.append(static_cast<char>((usernameLength >> 8) & 0xFF));
    payload.append(static_cast<char>(usernameLength & 0xFF));
    payload.append(usernameBytes);

    Protocol::ProtocolHeader header(Protocol::MessageType::REGISTER_HOST);
    header.payloadLength = static_cast<uint32_t>(payload.size());

    QByteArray packet = Protocol::ProtocolSerializer::serializeHeader(header);
    packet.append(payload);

    if (m_relayClient->sendRawPacket(packet) < 0) {
        qWarning() << "[ScreenStreamSender] Failed to send REGISTER_HOST.";
        return;
    }

    qDebug() << "[ScreenStreamSender] REGISTER_HOST sent for" << m_childUsername;
}

void ScreenStreamSender::onRelayBytesReceived(const QByteArray &data)
{
    const Protocol::RdtpStreamParser::FeedResult result = m_streamParser.feed(data);
    if (result.error != Protocol::RdtpStreamParser::Error::None) {
        qWarning() << "[ScreenStreamSender] Invalid RDTP data received from Relay.";
        return;
    }

    for (const Protocol::RdtpStreamParser::Message &message : result.messages) {
        if (message.header.type == Protocol::MessageType::SESSION_REQUEST) {
            handleSessionRequest(message);
            continue;
        }

        if (message.header.type != Protocol::MessageType::REGISTER_ACK)
            continue;

        const bool validAck = message.header.flags == 0
                && message.header.sessionId == 0
                && message.header.sequenceNumber == 0
                && message.header.payloadLength == 1
                && message.payload.size() == 1
                && (message.payload.at(0) == 0 || message.payload.at(0) == 1);

        if (!validAck) {
            m_registered = false;
            qWarning() << "[ScreenStreamSender] Malformed REGISTER_ACK.";
            continue;
        }

        m_registered = message.payload.at(0) == 1;
        qDebug() << "[ScreenStreamSender] Registration"
                 << (m_registered ? "accepted." : "rejected.");
    }
}

void ScreenStreamSender::handleSessionRequest(
        const Protocol::RdtpStreamParser::Message &message)
{
    const bool validRequest = message.header.flags == 0
            && message.header.sessionId != 0
            && message.header.sequenceNumber == 0
            && message.header.payloadLength == 0
            && message.payload.isEmpty();

    if (!validRequest) {
        qWarning() << "[ScreenStreamSender] SESSION_REQUEST khong hop le.";
        return;
    }

    if (m_currentSessionId != 0) {
        sendSessionResponse(Protocol::MessageType::SESSION_REJECT,
                            message.header.sessionId);
        qWarning() << "[ScreenStreamSender] Da co phien, gui SESSION_REJECT cho sessionId="
                   << message.header.sessionId;
        return;
    }

    if (!sendSessionResponse(Protocol::MessageType::SESSION_ACCEPT,
                             message.header.sessionId)) {
        qWarning() << "[ScreenStreamSender] Khong the gui SESSION_ACCEPT.";
        return;
    }

    m_currentSessionId = message.header.sessionId;
    qDebug() << "[ScreenStreamSender] Da gui SESSION_ACCEPT, sessionId="
             << m_currentSessionId;
}

bool ScreenStreamSender::sendSessionResponse(Protocol::MessageType type,
                                             uint64_t sessionId)
{
    Protocol::ProtocolHeader header(type);
    header.sessionId = sessionId;

    const QByteArray packet = Protocol::ProtocolSerializer::serializeHeader(header);
    return m_relayClient->sendRawPacket(packet) >= 0;
}

// ---------------------------------------------------------------------------
// onTick()
// Called by the timer every STREAM_INTERVAL_MS.
//
// Policy:
//  1. Backpressure check — skip frame if socket output queue is too large.
//  2. Capture one frame.
//  3. JPEG encode.
//  4. Packetize into RDTP SCREEN_FRAME messages.
//  5. Send each packet via RelayClient::sendRawPacket().
//  6. Increment frameId once per successfully packetized frame.
//
// Any failure at steps 1-4 silently skips the current tick.
// A failure at step 5 (sendRawPacket returns -1) aborts sending the rest
// of the current frame's chunks and skips the frameId increment.
// ---------------------------------------------------------------------------

void ScreenStreamSender::onTick()
{
    // --- 1. Backpressure check ---------------------------------------------
    const qint64 pending = m_relayClient->pendingBytes();
    if (pending > MAX_PENDING_BYTES) {
        qDebug() << "[ScreenStreamSender] Backpressure: skipping frame"
                 << m_frameId << " pending=" << pending << "bytes";
        return;
    }

    // --- 2. Capture --------------------------------------------------------
    static ScreenCapture capture;   // stateless — reuse across ticks
    QImage frame = capture.capture();
    if (frame.isNull()) {
        qWarning() << "[ScreenStreamSender] Capture failed, skipping frame.";
        return;
    }

    // --- 3. Encode ---------------------------------------------------------
    QByteArray encoded = ScreenEncoder::encodeJpeg(frame);
    if (encoded.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Encode failed, skipping frame.";
        return;
    }

    // --- 4. Packetize ------------------------------------------------------
    const QList<QByteArray> packets =
        ScreenFramePacketizer::packetize(encoded, m_frameId);
    if (packets.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Packetize returned empty, skipping frame.";
        return;
    }

    // --- 5. Send -----------------------------------------------------------
    for (const QByteArray &pkt : packets) {
        const qint64 written = m_relayClient->sendRawPacket(pkt);
        if (written < 0) {
            qWarning() << "[ScreenStreamSender] sendRawPacket failed (not connected?)."
                       << "Aborting frame" << m_frameId;
            return;   // do NOT increment frameId — frame was not fully sent
        }
    }

    // --- 6. Advance frameId ------------------------------------------------
    ++m_frameId;
}
