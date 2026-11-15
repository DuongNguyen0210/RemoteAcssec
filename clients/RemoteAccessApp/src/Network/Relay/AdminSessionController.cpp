#include "AdminSessionController.h"

#include "Network/Relay/RelayClient.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Protocol/ScreenFramePacketizer.h"

#include <QtEndian>
#include <QDebug>
#include <QUuid>
#include <QTimer>
#include <QBuffer>
#include <QImageReader>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
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
    auto *pruneTimer = new QTimer(this);
    pruneTimer->setInterval(1000);
    connect(pruneTimer, &QTimer::timeout, this, &AdminSessionController::pruneFrames);
    pruneTimer->start();
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
        emit requestFailed(targetAgentSessionId, QStringLiteral("ID phien may khong hop le."));
        return;
    }
    if (m_requestPending || m_activeSessionId != 0) {
        emit requestFailed(targetAgentSessionId, QStringLiteral("ADMIN da co phien dang cho hoac dang hoat dong."));
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

void AdminSessionController::disconnectSession()
{
    if (!m_activeSessionId) return;
    Protocol::ProtocolHeader header(Protocol::MessageType::DISCONNECT);
    header.sessionId = m_activeSessionId;
    const QByteArray packet = Protocol::ProtocolSerializer::serializeHeader(header);
    if (m_relayClient->sendRawPacket(packet) != packet.size()) {
        handleFatalRelayTermination(QStringLiteral("Không thể gửi yêu cầu ngắt phiên."));
        return;
    }
    const quint64 sessionId = m_activeSessionId;
    const QString agentId = m_activeAgentSessionId;
    m_activeSessionId = 0;
    m_activeAgentSessionId.clear();
    resetFrames();
    emit sessionEnded(sessionId, agentId, QStringLiteral("Đã ngắt kết nối."));
}

void AdminSessionController::onRelayConnected()
{
    m_connected = true;
    m_connecting = false;
    m_streamParser = Protocol::RdtpStreamParser{};
    resetFrames();

    if (m_requestPending)
        sendConnectRequest();
}

void AdminSessionController::onRelayDisconnected()
{
    handleFatalRelayTermination(QStringLiteral("Ket noi Relay da dong."));
}

void AdminSessionController::onRelayError(const QString &message, QAbstractSocket::SocketError socketError)
{
    if (!isFatalTransportError(socketError)) return;
    handleFatalRelayTermination(QStringLiteral("Loi ket noi Relay: %1.").arg(message));
}

void AdminSessionController::sendConnectRequest()
{
    if (!m_connected || !m_requestPending)
        return;

    const QByteArray payload = Protocol::relayAuthPayload(ApiClient::instance().getToken(), m_pendingAgentSessionId);
    if (payload.isEmpty()) {
        failPendingRequest(QStringLiteral("Thieu token dang nhap."));
        return;
    }

    Protocol::ProtocolHeader header(Protocol::MessageType::CONNECT_REQUEST);
    header.payloadLength = static_cast<uint32_t>(payload.size());

    QByteArray packet = Protocol::ProtocolSerializer::serializeHeader(header);
    packet.append(payload);

    if (m_relayClient->sendRawPacket(packet) < 0) {
        failPendingRequest(QStringLiteral("Khong the gui CONNECT_REQUEST."));
        return;
    }

    qDebug() << "[AdminSessionController] Da gui CONNECT_REQUEST cho"
             << m_pendingAgentSessionId;
}

void AdminSessionController::onRelayBytesReceived(const QByteArray &data)
{
    const Protocol::RdtpStreamParser::FeedResult result = m_streamParser.feed(data);
    if (result.error != Protocol::RdtpStreamParser::Error::None) {
        handleFatalRelayTermination(QStringLiteral("Du lieu RDTP tu Relay khong hop le."));
        return;
    }

    for (const Protocol::RdtpStreamParser::Message &message : result.messages) {
        if (message.header.type == Protocol::MessageType::SCREEN_FRAME) {
            if (m_activeSessionId == 0 || m_activeSessionId != message.header.sessionId)
            {
                continue;
            }
            const auto frame = m_frameReassembler.feed(message.payload);
            if (frame) {
                emit frameReceived(frame->frameId, frame->data);
                decodeFrame(frame->frameId, frame->data);
            }
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

        resetFrames();
        m_activeSessionId = static_cast<quint64>(message.header.sessionId);
        m_activeAgentSessionId = m_pendingAgentSessionId;
        m_requestPending = false;
        m_pendingAgentSessionId.clear();
        emit sessionEstablished(m_activeSessionId, m_activeAgentSessionId);
    }
}

void AdminSessionController::resetFrames()
{
    ++m_generation;
    m_frameReassembler.clear();
    m_pendingJpeg.clear();
}

void AdminSessionController::pruneFrames()
{
    m_frameReassembler.prune();
}

void AdminSessionController::decodeFrame(quint32 frameId, QByteArray jpeg)
{
    if (m_decoding) {
        m_pendingFrameId = frameId;
        m_pendingJpeg = std::move(jpeg);
        return;
    }
    m_decoding = true;
    const quint64 generation = m_generation;
    const quint64 sessionId = m_activeSessionId;
    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, generation, sessionId] {
        const QImage image = watcher->result();
        watcher->deleteLater();
        m_decoding = false;
        // Publish completed work even if a newer frame is queued. Otherwise a slow
        // decoder can starve the viewer forever while incoming frames keep advancing.
        if (generation == m_generation && sessionId == m_activeSessionId && sessionId && !image.isNull())
            emit frameReady(sessionId, image);
        if (!m_pendingJpeg.isEmpty() && m_activeSessionId) {
            QByteArray next = std::move(m_pendingJpeg);
            m_pendingJpeg.clear();
            decodeFrame(m_pendingFrameId, std::move(next));
        }
    });
    watcher->setFuture(QtConcurrent::run([jpeg = std::move(jpeg)]() mutable {
        QBuffer buffer(&jpeg);
        if (!buffer.open(QIODevice::ReadOnly)) return QImage();
        QImageReader reader(&buffer, "jpeg");
        const QSize dimensions = reader.size();
        if (!dimensions.isValid() || dimensions.width() > 8192 || dimensions.height() > 8192
                || qint64(dimensions.width()) * dimensions.height() > 16777216) return QImage();
        return reader.read();
    }));
}

void AdminSessionController::failPendingRequest(const QString &reason)
{
    if (!m_requestPending)
        return;
    const QString failedAgentSessionId  = m_pendingAgentSessionId;

    m_requestPending = false;
    m_pendingAgentSessionId.clear();
    emit requestFailed(failedAgentSessionId, reason);
}

void AdminSessionController::handleFatalRelayTermination(const QString &reason)
{
    const QString tmp_activeAgentSessionId = m_activeAgentSessionId;
    const QString tmp_pendingAgentSessionId = m_pendingAgentSessionId;
    const quint64 tmp_activeSessionId = m_activeSessionId;
    const bool tmp_requestPending = m_requestPending;

    m_streamParser = Protocol::RdtpStreamParser{};
    resetFrames();
    m_activeAgentSessionId.clear();
    m_pendingAgentSessionId.clear();
    m_activeSessionId = 0;
    m_connected = m_connecting = m_requestPending = false;

    m_relayClient->DisconnectFromServer();

    if (tmp_requestPending)
    {
        emit requestFailed(tmp_pendingAgentSessionId, reason);
    }

    else if (tmp_activeSessionId != 0)
    {
        emit sessionEnded(tmp_activeSessionId, tmp_activeAgentSessionId, reason);
    }
}

bool AdminSessionController::isFatalTransportError(QAbstractSocket::SocketError socketError)
{
    switch(socketError)
    {
        case QAbstractSocket::TemporaryError: //Lỗi tạm thời (do mạng, điều kiện hiện tại chưa cho hoàn tất thao tác,...)
        case QAbstractSocket::UnfinishedSocketOperationError: // Thao tác socket trước đó vẫn chưa hoàn tất, vẫn đang chạy nền
        case QAbstractSocket::OperationError: // Đã yêu cầu một thao tác khi socket đang ở trạng thái không cho phép thao tác đó
            return false;

        default: return true;
    }
}
