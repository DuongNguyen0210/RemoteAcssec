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
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(15000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        failPendingRequest(QStringLiteral("Hết thời gian chờ tạo phiên."));
    });
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
    requestSession(targetAgentSessionId, qEnvironmentVariable("REMOTE_RELAY_HOST", RELAY_HOST),
            qEnvironmentVariable("REMOTE_RELAY_PORT", QString::number(RELAY_PORT)).toUShort());
}

void AdminSessionController::requestSession(const QString &targetAgentSessionId, const QString &host, quint16 port)
{
    if (host.trimmed().isEmpty() || port == 0) {
        emit sessionFailed(QStringLiteral("Endpoint Relay không hợp lệ."));
        return;
    }
    if (QUuid(targetAgentSessionId).isNull()) {
        emit sessionFailed(QStringLiteral("ID phiên máy không hợp lệ."));
        return;
    }

    if (m_requestPending || m_activeSessionId != 0) {
        emit sessionFailed(QStringLiteral("Đang có phiên kết nối."));
        return;
    }

    endSession();
    m_pendingAgentSessionId = targetAgentSessionId;
    m_requestPending = true;
    m_connecting = true;
    m_timeout.start();
    m_relayClient->ConnectToServer(host, port);
}

void AdminSessionController::endSession()
{
    m_timeout.stop();
    m_requestPending = false;
    m_pendingAgentSessionId.clear();
    const bool wasActive = m_activeSessionId != 0;
    m_activeSessionId = 0;
    m_connected = false;
    m_connecting = false;
    m_streamParser = {};
    m_relayClient->DisconnectFromServer();
    if (wasActive) emit sessionEnded();
}

bool AdminSessionController::sendInput(Protocol::MessageType type, const QByteArray &payload)
{
    using T = Protocol::MessageType;
    if (!m_connected || m_activeSessionId == 0 || !(type == T::MOUSE_MOVE || type == T::MOUSE_BUTTON_DOWN
            || type == T::MOUSE_BUTTON_UP || type == T::MOUSE_WHEEL || type == T::KEY_PRESS || type == T::KEY_RELEASE)
            || payload.size() > Protocol::MAX_PAYLOAD_LENGTH || m_relayClient->pendingBytes() > 1024 * 1024) return false;
    Protocol::ProtocolHeader header(type);
    header.sessionId = m_activeSessionId;
    header.payloadLength = static_cast<uint32_t>(payload.size());
    const auto packet = Protocol::ProtocolSerializer::serializeHeader(header) + payload;
    return m_relayClient->sendRawPacket(packet) == packet.size();
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
    m_timeout.stop();
    const bool hadActiveSession = m_activeSessionId != 0;

    m_connected = false;
    m_connecting = false;
    m_activeSessionId = 0;
    m_streamParser = Protocol::RdtpStreamParser{};

    if (m_requestPending) {
        failPendingRequest(QStringLiteral("Kết nối đã đóng trước khi tạo phiên."));
    } else if (hadActiveSession) {
        emit sessionEnded();
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
        if (m_requestPending) failPendingRequest(QStringLiteral("Dữ liệu kết nối không hợp lệ."));
        else endSession();
        return;
    }

    for (const Protocol::RdtpStreamParser::Message &message : result.messages) {
        if (message.header.type == Protocol::MessageType::SCREEN_FRAME && m_activeSessionId != 0
                && message.header.sessionId == m_activeSessionId) {
            emit screenReceived(message.header, message.payload);
            continue;
        }
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

        m_timeout.stop();
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
    endSession();
    emit sessionFailed(reason);
}
