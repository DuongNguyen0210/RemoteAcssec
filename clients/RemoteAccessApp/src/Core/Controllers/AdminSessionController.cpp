#include "AdminSessionController.h"

#include "Network/Client/relayclient.h"
#include "Network/Protocol/protocolserializer.h"

#include <QDebug>

namespace {

const QString RELAY_HOST = QStringLiteral("localhost");
constexpr quint16 RELAY_PORT = 8080;

}

AdminSessionController::AdminSessionController(QObject *parent)
    : QObject(parent)
    , m_relayClient(new RelayClient(this))
    , m_activeSessionId(0)
    , m_connected(false)
    , m_connecting(false)
    , m_requestPending(false)
{
    connect(m_relayClient, &RelayClient::connected,
            this, &AdminSessionController::onRelayConnected);
    connect(m_relayClient, &RelayClient::disconnected,
            this, &AdminSessionController::onRelayDisconnected);
    connect(m_relayClient, &RelayClient::bytesReceived,
            this, &AdminSessionController::onRelayBytesReceived);
    connect(m_relayClient, &RelayClient::transportError,
            this, &AdminSessionController::onRelayError);
}

void AdminSessionController::requestSession(const QString &targetChildUsername)
{
    const QByteArray usernameBytes = targetChildUsername.toUtf8();
    if (usernameBytes.isEmpty() || usernameBytes.size() > 200) {
        emit sessionFailed(QStringLiteral("childUsername phai dai tu 1 den 200 byte UTF-8."));
        return;
    }

    if (m_requestPending || m_activeSessionId != 0) {
        emit sessionFailed(QStringLiteral("ADMIN da co phien dang cho hoac dang hoat dong."));
        return;
    }

    m_pendingTargetUsername = targetChildUsername;
    m_requestPending = true;

    if (m_connected) {
        sendConnectRequest();
        return;
    }

    if (!m_connecting) {
        m_connecting = true;
        m_relayClient->ConnectToServer(RELAY_HOST, RELAY_PORT);
    }
}

bool AdminSessionController::sendSessionMessage(
        Protocol::MessageType type, const QByteArray &payload)
{
    if (!m_connected || m_activeSessionId == 0
            || static_cast<quint64>(payload.size()) > Protocol::MAX_PAYLOAD_LENGTH) {
        return false;
    }

    Protocol::ProtocolHeader header(type);
    header.payloadLength = static_cast<uint32_t>(payload.size());
    header.sessionId = static_cast<uint64_t>(m_activeSessionId);

    QByteArray packet = Protocol::ProtocolSerializer::serializeHeader(header);
    packet.append(payload);
    return m_relayClient->sendRawPacket(packet) >= 0;
}

void AdminSessionController::onRelayConnected()
{
    m_connected = true;
    m_connecting = false;
    m_streamParser = Protocol::RdtpStreamParser{};

    if (m_requestPending)
        sendConnectRequest();
}

void AdminSessionController::onRelayDisconnected()
{
    const bool hadActiveSession = m_activeSessionId != 0;

    m_connected = false;
    m_connecting = false;
    m_activeSessionId = 0;
    m_streamParser = Protocol::RdtpStreamParser{};

    if (m_requestPending) {
        failPendingRequest(QStringLiteral("Ket noi Relay da dong truoc khi tao phien."));
    } else if (hadActiveSession) {
        emit sessionFailed(QStringLiteral("Ket noi Relay cua phien dang hoat dong da dong."));
    }
}

void AdminSessionController::onRelayError(const QString &message)
{
    m_connecting = false;
    if (m_requestPending)
        failPendingRequest(QStringLiteral("Loi ket noi Relay: %1").arg(message));
}

void AdminSessionController::sendConnectRequest()
{
    if (!m_connected || !m_requestPending)
        return;

    const QByteArray usernameBytes = m_pendingTargetUsername.toUtf8();
    QByteArray payload;
    payload.reserve(2 + usernameBytes.size());

    const quint16 usernameLength = static_cast<quint16>(usernameBytes.size());
    payload.append(static_cast<char>((usernameLength >> 8) & 0xFF));
    payload.append(static_cast<char>(usernameLength & 0xFF));
    payload.append(usernameBytes);

    Protocol::ProtocolHeader header(Protocol::MessageType::CONNECT_REQUEST);
    header.payloadLength = static_cast<uint32_t>(payload.size());

    QByteArray packet = Protocol::ProtocolSerializer::serializeHeader(header);
    packet.append(payload);

    if (m_relayClient->sendRawPacket(packet) < 0) {
        failPendingRequest(QStringLiteral("Khong the gui CONNECT_REQUEST."));
        return;
    }

    qDebug() << "[AdminSessionController] Da gui CONNECT_REQUEST cho"
             << m_pendingTargetUsername;
}

void AdminSessionController::onRelayBytesReceived(const QByteArray &data)
{
    const Protocol::RdtpStreamParser::FeedResult result = m_streamParser.feed(data);
    if (result.error != Protocol::RdtpStreamParser::Error::None) {
        failPendingRequest(QStringLiteral("Du lieu RDTP tu Relay khong hop le."));
        return;
    }

    for (const Protocol::RdtpStreamParser::Message &message : result.messages) {
        if (message.header.type != Protocol::MessageType::CONNECT_RESULT)
            emit sessionProtocolReceived(message);

        if (message.header.type != Protocol::MessageType::CONNECT_RESULT)
            continue;

        const bool validCommonFields = message.header.flags == 0
                && message.header.sequenceNumber == 0
                && message.header.payloadLength == 1
                && message.payload.size() == 1
                && (message.payload.at(0) == 0 || message.payload.at(0) == 1);

        if (!validCommonFields || !m_requestPending) {
            if (m_requestPending)
                failPendingRequest(QStringLiteral("CONNECT_RESULT khong hop le."));
            continue;
        }

        const bool accepted = message.payload.at(0) == 1;
        const bool validResult = (accepted && message.header.sessionId != 0)
                || (!accepted && message.header.sessionId == 0);
        if (!validResult) {
            failPendingRequest(QStringLiteral("CONNECT_RESULT khong hop le."));
            continue;
        }

        if (!accepted) {
            failPendingRequest(QStringLiteral("Relay tu choi yeu cau ket noi CHILD."));
            continue;
        }

        m_activeSessionId = static_cast<quint64>(message.header.sessionId);
        m_requestPending = false;
        m_pendingTargetUsername.clear();
        emit sessionEstablished(m_activeSessionId);
    }
}

void AdminSessionController::failPendingRequest(const QString &reason)
{
    if (!m_requestPending)
        return;

    m_requestPending = false;
    m_pendingTargetUsername.clear();
    emit sessionFailed(reason);
}
