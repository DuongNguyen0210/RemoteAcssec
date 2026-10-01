#include "ChildSessionController.h"
#include "RelayClient.h"
#include "Network/Http/ApiClient.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Protocol/RelayAuthPayload.h"

namespace {
bool isInput(Protocol::MessageType type) {
    using T = Protocol::MessageType;
    return type == T::MOUSE_MOVE || type == T::MOUSE_BUTTON_DOWN || type == T::MOUSE_BUTTON_UP
            || type == T::MOUSE_WHEEL || type == T::KEY_PRESS || type == T::KEY_RELEASE;
}
}

ChildSessionController::ChildSessionController(RelayEndpointProvider *provider, QObject *parent)
    : QObject(parent), m_provider(provider), m_client(new RelayClient(this)) {
    m_retry.setSingleShot(true);
    m_retry.setInterval(1500);
    m_renew.setInterval(10000);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(10000);
    connect(&m_retry, &QTimer::timeout, this, &ChildSessionController::allocate);
    connect(&m_renew, &QTimer::timeout, this, &ChildSessionController::allocate);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("Relay không phản hồi.")); });
    connect(m_client, &RelayClient::connected, this, &ChildSessionController::connected);
    connect(m_client, &RelayClient::bytesReceived, this, &ChildSessionController::receive);
    connect(m_client, &RelayClient::disconnected, this, [this] {
        reset(QStringLiteral("Kết nối Relay đã đóng."));
        scheduleReconnect();
    });
    connect(m_client, &RelayClient::transportError, this, &ChildSessionController::fail);
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
            self->fail(error.isEmpty() ? QStringLiteral("Endpoint Relay hết hạn.") : error);
            return;
        }
        if (self->m_registered && self->m_endpoint.sameAddress(endpoint)) {
            self->m_endpoint = endpoint;
            return;
        }
        self->reset(QStringLiteral("Endpoint Relay đã thay đổi."));
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
    if (payload.isEmpty() || !sendControl(Protocol::MessageType::REGISTER_HOST, 0, payload))
        fail(QStringLiteral("Không đăng ký được với Relay."));
}

bool ChildSessionController::sendControl(Protocol::MessageType type, quint64 id, const QByteArray &payload) {
    Protocol::ProtocolHeader header(type);
    header.sessionId = id;
    header.payloadLength = static_cast<uint32_t>(payload.size());
    auto packet = Protocol::ProtocolSerializer::serializeHeader(header) + payload;
    return m_client->sendRawPacket(packet) == packet.size();
}

void ChildSessionController::receive(const QByteArray &bytes) {
    const auto result = m_parser.feed(bytes);
    if (result.error != Protocol::RdtpStreamParser::Error::None) {
        fail(QStringLiteral("Gói tin Relay không hợp lệ.")); return;
    }
    using T = Protocol::MessageType;
    for (const auto &message : result.messages) {
        const auto &h = message.header;
        if (h.type == T::REGISTER_ACK) {
            if (!m_connecting || h.flags != 0 || h.sessionId != 0 || h.sequenceNumber != 0
                    || message.payload.size() != 1 || message.payload.at(0) != 1) {
                fail(QStringLiteral("Relay từ chối đăng ký.")); return;
            }
            m_registered = true;
            m_connecting = false;
            m_timeout.stop();
            if (m_endpoint.expiresAt != 0) m_renew.start();
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
                fail(QStringLiteral("Không chấp nhận được phiên.")); return;
            }
            m_sessionId = h.sessionId;
            emit sessionStarted(m_sessionId);
        } else if (isInput(h.type) && m_sessionId != 0 && h.sessionId == m_sessionId) {
            emit inputReceived(h, message.payload);
        }
    }
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

    return pre_active;
}


void ChildSessionController::reset(const QString &reason) {
    if (resetRelayState()) emit sessionEnded(reason);
}

void ChildSessionController::fail(const QString &reason) {
    ++m_generation;
    m_allocating = false;
    bool isPreActive = resetRelayState(); // Lưu lại việc trước khi reset có session ACTIVE hay không, rồi dọn state Relay
    m_client->DisconnectFromServer();
    if (isPreActive) emit sessionEnded(reason);
    else emit sessionFailed(reason);
    scheduleReconnect();
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
    if (!m_running || m_retry.isActive()) return;
    m_retry.start();
}

