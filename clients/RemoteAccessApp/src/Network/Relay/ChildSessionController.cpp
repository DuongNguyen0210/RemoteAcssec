#include "ChildSessionController.h"
#include "RelayClient.h"
#include "Network/Http/ApiClient.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Protocol/RelayAuthPayload.h"
#include <QRandomGenerator>

namespace {
QByteArray controlPacket(Protocol::MessageType type, quint64 id, const QByteArray &payload = {},
                         quint16 flags = 0) {
    Protocol::ProtocolHeader header(type);
    header.sessionId = id;
    header.flags = flags;
    header.payloadLength = static_cast<uint32_t>(payload.size());
    return Protocol::ProtocolSerializer::serializeHeader(header) + payload;
}

bool isInput(Protocol::MessageType type) {
    using T = Protocol::MessageType;
    return type == T::MOUSE_MOVE || type == T::MOUSE_BUTTON_DOWN || type == T::MOUSE_BUTTON_UP
            || type == T::MOUSE_WHEEL || type == T::KEY_PRESS || type == T::KEY_RELEASE;
}
}

ChildSessionController::ChildSessionController(RelayEndpointProvider *provider, QObject *parent)
    : QObject(parent), m_provider(provider), m_client(new RelayClient(this)) {
    m_retry.setSingleShot(true);
    m_renew.setInterval(10000);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(30000);
    m_frameAckTimeout.setSingleShot(true);
    m_frameAckTimeout.setInterval(30000);
    connect(&m_retry, &QTimer::timeout, this, &ChildSessionController::allocate);
    connect(&m_renew, &QTimer::timeout, this, &ChildSessionController::allocate);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("Relay không phản hồi.")); });
    connect(&m_frameAckTimeout, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("ADMIN không xác nhận khung hình."));
    });
    connect(m_client, &RelayClient::connected, this, &ChildSessionController::connected);
    connect(m_client, &RelayClient::bytesReceived, this, &ChildSessionController::receive);
    connect(m_client, &RelayClient::disconnected, this, [this] {
        if (!m_connecting && !m_registered)
            return;
        const bool wasConnecting = m_connecting;
        ++m_generation;
        m_allocating = false;
        const auto reason = QStringLiteral("Kết nối Relay đã đóng.");
        reset(reason);
        if (wasConnecting) emit sessionFailed(reason);
        scheduleReconnect();
    });
    connect(m_client, &RelayClient::transportError, this, [this](const QString &reason) {
        if (m_connecting || m_registered)
            fail(reason);
    });
}

void ChildSessionController::start() {
    if (m_running) return;
    m_running = true;
    allocate();
}

void ChildSessionController::stop() {
    m_running = false;
    ++m_generation;
    m_allocating = false;
    m_retry.stop();
    m_retryAttempt = 0;
    m_renew.stop();
    m_timeout.stop();
    reset(QStringLiteral("Đã dừng kết nối Relay."));
    m_client->DisconnectFromServer();
}

//xin Relay endpoint trước khi mở kết nối TCP thật sự (nhận về host + post để biết tới relay nào)
void ChildSessionController::allocate() {
    if (!m_running || m_allocating || m_connecting) return;
    if (!m_provider) { fail(QStringLiteral("Thiếu cấu hình Relay.")); return; }
    m_allocating = true;
    const auto generation = m_generation;
    QPointer<ChildSessionController> self(this);
    m_provider->allocate([self, generation](const RelayEndpoint &endpoint, const QString &error) {
        if (!self || !self->m_running || self->m_generation != generation) return;
        self->m_allocating = false;
        if (!error.isEmpty() || !endpoint.isValid()) {
            if (self->m_registered && self->m_endpoint.isValid()) {
                return;
            }
            self->fail(error.isEmpty() ? QStringLiteral("Endpoint Relay hết hạn.") : error);
            return;
        }
        if (self->m_registered && self->m_endpoint.sameAddress(endpoint)) {
            self->m_endpoint = endpoint;
            return;
        }
        self->reset(QStringLiteral("Endpoint Relay đã thay đổi."));
        if (!self || !self->m_running || self->m_generation != generation) return;
        self->m_client->DisconnectFromServer();
        self->m_retry.stop();
        self->m_endpoint = endpoint;
        self->m_connecting = true;
        self->m_timeout.start();
        self->m_client->ConnectToServer(endpoint.host, endpoint.port);
    });
}

void ChildSessionController::connected() {
    if (!m_running) { m_client->DisconnectFromServer(); return; }
    const auto payload = Protocol::relayAuthPayload(ApiClient::instance().getToken());
    if (payload.isEmpty()) {
        fail(QStringLiteral("Không đăng ký được với Relay."));
        return;
    }
    using T = Protocol::MessageType;
    const auto packet = controlPacket(T::SCREEN_FRAME_ACK, 0, {}, Protocol::SCREEN_FRAME_ACK_CAPABILITY_FLAG)
            + controlPacket(T::REGISTER_HOST, 0, payload);
    const auto generation = m_generation;
    if (m_client->sendRawPacket(packet) != packet.size() && m_generation == generation)
        fail(QStringLiteral("Không đăng ký được với Relay."));
}

bool ChildSessionController::sendControl(Protocol::MessageType type, quint64 id, const QByteArray &payload) {
    const auto packet = controlPacket(type, id, payload);
    return m_client->sendRawPacket(packet) == packet.size();
}

void ChildSessionController::receive(const QByteArray &bytes) {
    const auto result = m_parser.feed(bytes);
    if (result.error != Protocol::RdtpStreamParser::Error::None) {
        fail(QStringLiteral("Gói tin Relay không hợp lệ.")); return;
    }
    using T = Protocol::MessageType;
    const auto generation = m_generation;
    for (const auto &message : result.messages) {
        if (generation != m_generation)
            return;
        const auto &h = message.header;
        if (h.type == T::REGISTER_ACK) {
            if (!m_connecting || h.flags != 0 || h.sessionId != 0 || h.sequenceNumber != 0
                    || message.payload.size() != 1 || message.payload.at(0) != 1) {
                fail(QStringLiteral("Relay từ chối đăng ký.")); return;
            }
            m_registered = true;
            m_connecting = false;
            m_retryAttempt = 0;
            m_timeout.stop();
            if (m_endpoint.expiresAt != 0) m_renew.start();
            emit relayRegistered();
        } else if (h.type == T::SESSION_REQUEST) {
            if (!m_registered || h.sessionId == 0 || h.flags != 0 || h.sequenceNumber != 0
                    || !message.payload.isEmpty()) {
                fail(QStringLiteral("Yêu cầu phiên không hợp lệ.")); return;
            }
            if (m_sessionId != 0) {
                sendControl(T::SESSION_REJECT, h.sessionId);
                continue;
            }
            if (!sendControl(T::SESSION_ACCEPT, h.sessionId)) {
                if (generation == m_generation) fail(QStringLiteral("Không chấp nhận được phiên."));
                return;
            }
            m_sessionId = h.sessionId;
            emit sessionStarted(m_sessionId);
        } else if (h.type == T::SCREEN_FRAME_ACK && m_sessionId != 0 && h.sessionId == m_sessionId
                   && message.payload.isEmpty()) {
            if (h.flags == Protocol::SCREEN_FRAME_ACK_CAPABILITY_FLAG && h.sequenceNumber == 0) {
                m_screenFrameAcksEnabled = true;
            } else if (h.flags == 0 && m_inFlightFrame && h.sequenceNumber == *m_inFlightFrame) {
                m_inFlightFrame.reset();
                m_frameAckTimeout.stop();
            }
        } else if (h.type == T::DISCONNECT) {
            if (m_sessionId != 0 && h.sessionId == m_sessionId && h.flags == 0
                    && h.sequenceNumber == 0 && message.payload.isEmpty()) {
                m_sessionId = 0;
                clearFrameWindow();
                emit sessionEnded(QStringLiteral("ADMIN đã ngắt phiên."));
            }
        } else if (isInput(h.type) && m_sessionId != 0 && h.sessionId == m_sessionId) {
            emit inputReceived(h, message.payload);
        }
    }
}

void ChildSessionController::clearFrameWindow()
{
    m_frameAckTimeout.stop();
    m_inFlightFrame.reset();
    m_screenFrameAcksEnabled = false;
}

bool ChildSessionController::resetRelayState()
{
    const bool pre_active = (m_sessionId != 0);
    m_sessionId = 0;
    m_registered = 0;
    m_connecting = 0;
    m_parser = Protocol::RdtpStreamParser{};
    m_timeout.stop();
    m_renew.stop();
    clearFrameWindow();

    return pre_active;
}


void ChildSessionController::reset(const QString &reason) {
    if (resetRelayState()) emit sessionEnded(reason);
}

void ChildSessionController::fail(const QString &reason) {
    ++m_generation;
    m_allocating = false;
    const bool hadSession = resetRelayState();
    m_client->DisconnectFromServer();
    if (hadSession) emit sessionEnded(reason);
    else emit sessionFailed(reason);
    scheduleReconnect();
}

bool ChildSessionController::canSendScreenFrame() const
{
    return m_sessionId != 0 && !m_inFlightFrame && pendingBytes() == 0;
}

bool ChildSessionController::sendScreenFrame(quint32 frameId, const QList<QByteArray> &packets)
{
    if (packets.isEmpty() || !canSendScreenFrame())
        return false;
    const auto generation = m_generation;
    for (const auto &packet : packets) {
        if (sendScreenPacket(packet) != packet.size()) {
            if (generation == m_generation)
                fail(QStringLiteral("Không gửi được khung hình qua Relay."));
            return false;
        }
    }
    if (m_screenFrameAcksEnabled) {
        m_inFlightFrame = frameId;
        m_frameAckTimeout.start();
    }
    return true;
}

qint64 ChildSessionController::sendScreenPacket(const QByteArray &packet) {
    const auto header = Protocol::ProtocolSerializer::deserializeHeader(packet.left(Protocol::HEADER_SIZE));
    if (!m_registered || m_sessionId == 0 || !header || header->type != Protocol::MessageType::SCREEN_FRAME
            || header->magic != Protocol::PROTOCOL_MAGIC || header->version != Protocol::PROTOCOL_VERSION
            || header->payloadLength > Protocol::MAX_PAYLOAD_LENGTH
            || header->sessionId != m_sessionId || packet.size() != Protocol::HEADER_SIZE + header->payloadLength) return -1;
    return m_client->sendRawPacket(packet);
}

qint64 ChildSessionController::pendingBytes() const { return m_client->pendingBytes(); }

void ChildSessionController::scheduleReconnect()
{
    if (!m_running || m_allocating || m_connecting || m_registered || m_retry.isActive()) return;
    const int base = qMin(30000, 1500 * (1 << qMin(m_retryAttempt, 5)));
    m_retry.setInterval(qMin(30000, base + QRandomGenerator::global()->bounded(500)));
    if (m_retryAttempt < 5) ++m_retryAttempt;
    m_retry.start();
}

