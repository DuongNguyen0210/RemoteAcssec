#include "ScreenStreamSender.h"
#include "Network/Relay/ChildSessionController.h"

#include "ScreenCapture.h"
#include "ScreenEncoder.h"
#include "Network/Relay/RelayClient.h"
#include "Network/Protocol/ScreenFramePacketizer.h"
#include "Network/Protocol/ProtocolSerializer.h"

#include <QDebug>
#include "Network/Http/ApiClient.h"
#include "Network/Protocol/RelayAuthPayload.h"

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

ScreenStreamSender::ScreenStreamSender(ChildSessionController *session, QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_relayClient(session->relayClient())
    , m_frameId(1)
    , m_registered(false)
    , m_currentSessionId(0)
{
    m_timer->setInterval(STREAM_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &ScreenStreamSender::onTick);
    connect(session, &ChildSessionController::sessionStarted, this, [this](quint64 id) {
        m_currentSessionId = id;
        m_registered = true;
        m_timer->start();
    });
    connect(session, &ChildSessionController::sessionEnded, this, [this] {
        m_currentSessionId = 0;
        m_registered = false;
        m_timer->stop();
    });
}

ScreenStreamSender::~ScreenStreamSender()
{
    stop();
}

void ScreenStreamSender::start()
{
    m_running = true;
    m_relayClient->ConnectToServer(qEnvironmentVariable("REMOTE_RELAY_HOST", RELAY_HOST),
            static_cast<quint16>(qEnvironmentVariable("REMOTE_RELAY_PORT", QString::number(RELAY_PORT)).toUShort()));
    m_timer->start();
    qDebug() << "[ScreenStreamSender] Started."
             << "host=" << RELAY_HOST
             << "port=" << RELAY_PORT
             << "interval=" << STREAM_INTERVAL_MS << "ms";
}

void ScreenStreamSender::stop()
{
    m_running = false;
    m_relayClient->DisconnectFromServer();
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
    if (m_running) QTimer::singleShot(1000, this, [this]() { if (m_running) start(); });
    m_registered = false;
    m_currentSessionId = 0;
    m_streamParser = Protocol::RdtpStreamParser{};
}

void ScreenStreamSender::sendRegisterHost()
{
    const QByteArray payload = Protocol::relayAuthPayload(ApiClient::instance().getToken());
    if (payload.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Missing login token.";
        return;
    }

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

    if (!m_registered || m_currentSessionId != 0) {
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

void ScreenStreamSender::onTick()
{
    if (m_currentSessionId == 0)
        return;

    const qint64 pending = m_relayClient->pendingBytes();
    if (pending > MAX_PENDING_BYTES) {
        qDebug() << "[ScreenStreamSender] Backpressure: skipping frame"
                 << m_frameId << " pending=" << pending << "bytes";
        return;
    }

    static ScreenCapture capture;
    QImage frame = capture.capture();
    if (frame.isNull()) {
        qWarning() << "[ScreenStreamSender] Capture failed, skipping frame.";
        return;
    }

    QByteArray encoded = ScreenEncoder::encodeJpeg(frame);
    if (encoded.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Encode failed, skipping frame.";
        return;
    }

    const QList<QByteArray> packets =
        ScreenFramePacketizer::packetize(encoded, m_frameId, m_currentSessionId);
    if (packets.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Packetize returned empty, skipping frame.";
        return;
    }

    for (const QByteArray &pkt : packets) {
        const qint64 written = m_relayClient->sendRawPacket(pkt);
        if (written < 0) {
            qWarning() << "[ScreenStreamSender] sendRawPacket failed (not connected?)."
                       << "Aborting frame" << m_frameId;
            return;
        }
    }

    ++m_frameId;
}
