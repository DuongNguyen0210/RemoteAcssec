#include "RemoteInputReceiver.h"

#include "Network/Protocol/MouseProtocolCodec.h"

#include <QDebug>

namespace {

bool isMouseMessageType(Protocol::MessageType type)
{
    return type == Protocol::MessageType::MOUSE_MOVE
            || type == Protocol::MessageType::MOUSE_BUTTON_DOWN
            || type == Protocol::MessageType::MOUSE_BUTTON_UP
            || type == Protocol::MessageType::MOUSE_WHEEL;
}

}

RemoteInputReceiver::RemoteInputReceiver(QObject *parent)
    : QObject(parent)
    , m_activeSessionId(0)
{
}

void RemoteInputReceiver::setActiveSession(quint64 sessionId)
{
    m_activeSessionId = sessionId;
}

void RemoteInputReceiver::handleMessage(
        const Protocol::RdtpStreamParser::Message &message)
{
    if (!isMouseMessageType(message.header.type))
        return;

    if (m_activeSessionId == 0
            || message.header.sessionId == 0
            || message.header.sessionId != m_activeSessionId) {
        qWarning() << "[RemoteInputReceiver] Bo qua mouse message khong dung phien.";
        return;
    }

    if (message.header.flags != 0 || message.header.sequenceNumber != 0) {
        qWarning() << "[RemoteInputReceiver] Bo qua mouse header khong hop le.";
        return;
    }

    if (message.header.payloadLength != static_cast<uint32_t>(message.payload.size())) {
        qWarning() << "[RemoteInputReceiver] Bo qua mouse message sai payloadLength.";
        return;
    }

    const auto event = MouseProtocolCodec::decodePayload(
            message.header.type, message.payload);
    if (!event.has_value()) {
        qWarning() << "[RemoteInputReceiver] Bo qua mouse payload khong hop le.";
        return;
    }

    m_mouseInjector.inject(event.value());
}
