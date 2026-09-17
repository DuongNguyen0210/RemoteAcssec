#include "AdminSessionController.h"

#include "Network/Relay/RelayClient.h"
#include "Network/Protocol/ProtocolSerializer.h"

#include <QDebug>
#include <QUuid>
#include "Network/Http/ApiClient.h"
#include "Network/Protocol/RelayAuthPayload.h"

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

void AdminSessionController::requestSession(const QString &targetAgentSessionId)
{
    if (QUuid(targetAgentSessionId).isNull()) {
        emit sessionFailed(QStringLiteral("ID phiên máy không hợp lệ."));
        return;
    }

    if (m_requestPending || m_activeSessionId != 0) {
        emit sessionFailed(QStringLiteral("Đang có phiên kết nối."));
        return;
    }

    m_pendingAgentSessionId = targetAgentSessionId;
    m_requestPending = true;

    if (m_connected) {
        sendConnectRequest();
        return;
    }

    if (!m_connecting) {
        m_connecting = true;
        m_relayClient->ConnectToServer(qEnvironmentVariable("REMOTE_RELAY_HOST", RELAY_HOST),
                static_cast<quint16>(qEnvironmentVariable("REMOTE_RELAY_PORT", QString::number(RELAY_PORT)).toUShort()));
    }
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
        failPendingRequest(QStringLiteral("Kết nối đã đóng trước khi tạo phiên."));
    } else if (hadActiveSession) {
        emit sessionFailed(QStringLiteral("Phiên kết nối đã đóng."));
    }
}

void AdminSessionController::onRelayError(const QString &message)
{
    Q_UNUSED(message);
    m_connecting = false;
    if (m_requestPending)
        failPendingRequest(QStringLiteral("Không kết nối được máy chủ relay."));
}

void AdminSessionController::sendConnectRequest()
{
    if (!m_connected || !m_requestPending)
        return;

    const QByteArray payload = Protocol::relayAuthPayload(ApiClient::instance().getToken(), m_pendingAgentSessionId);
    if (payload.isEmpty()) {
        failPendingRequest(QStringLiteral("Vui lòng đăng nhập lại."));
        return;
    }

    Protocol::ProtocolHeader header(Protocol::MessageType::CONNECT_REQUEST);
    header.payloadLength = static_cast<uint32_t>(payload.size());

    QByteArray packet = Protocol::ProtocolSerializer::serializeHeader(header);
    packet.append(payload);

    if (m_relayClient->sendRawPacket(packet) < 0) {
        failPendingRequest(QStringLiteral("Không gửi được yêu cầu kết nối."));
        return;
    }

    qDebug() << "[AdminSessionController] Da gui CONNECT_REQUEST cho"
             << m_pendingAgentSessionId;
}

void AdminSessionController::onRelayBytesReceived(const QByteArray &data)
{
    const Protocol::RdtpStreamParser::FeedResult result = m_streamParser.feed(data);
    if (result.error != Protocol::RdtpStreamParser::Error::None) {
        failPendingRequest(QStringLiteral("Dữ liệu kết nối không hợp lệ."));
        return;
    }

    for (const Protocol::RdtpStreamParser::Message &message : result.messages) {
        if (message.header.type != Protocol::MessageType::CONNECT_RESULT)
            continue;

        const bool validCommonFields = message.header.flags == 0
                && message.header.sequenceNumber == 0
                && message.header.payloadLength == 1
                && message.payload.size() == 1
                && (message.payload.at(0) == 0 || message.payload.at(0) == 1);

        if (!validCommonFields || !m_requestPending) {
            if (m_requestPending)
                failPendingRequest(QStringLiteral("Phản hồi kết nối không hợp lệ."));
            continue;
        }

        const bool accepted = message.payload.at(0) == 1;
        const bool validResult = (accepted && message.header.sessionId != 0)
                || (!accepted && message.header.sessionId == 0);
        if (!validResult) {
            failPendingRequest(QStringLiteral("Phản hồi kết nối không hợp lệ."));
            continue;
        }

        if (!accepted) {
            failPendingRequest(QStringLiteral("Yêu cầu kết nối bị từ chối."));
            continue;
        }

        m_activeSessionId = static_cast<quint64>(message.header.sessionId);
        m_requestPending = false;
        m_pendingAgentSessionId.clear();
        emit sessionEstablished(m_activeSessionId);
    }
}

void AdminSessionController::failPendingRequest(const QString &reason)
{
    if (!m_requestPending)
        return;

    m_requestPending = false;
    m_pendingAgentSessionId.clear();
    emit sessionFailed(reason);
}
