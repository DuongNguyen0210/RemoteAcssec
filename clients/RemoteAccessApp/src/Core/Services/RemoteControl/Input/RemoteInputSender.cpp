#include "RemoteInputSender.h"

#include "Core/Controllers/AdminSessionController.h"
#include "Network/Protocol/MouseProtocolCodec.h"

#include <QDebug>

#include <optional>

namespace {

std::optional<Protocol::MessageType> messageTypeForAction(RemoteMouseAction action)
{
    switch (action) {
    case RemoteMouseAction::Move:
        return Protocol::MessageType::MOUSE_MOVE;
    case RemoteMouseAction::ButtonDown:
        return Protocol::MessageType::MOUSE_BUTTON_DOWN;
    case RemoteMouseAction::ButtonUp:
        return Protocol::MessageType::MOUSE_BUTTON_UP;
    case RemoteMouseAction::Wheel:
        return Protocol::MessageType::MOUSE_WHEEL;
    }

    return std::nullopt;
}

}

RemoteInputSender::RemoteInputSender(QObject *parent)
    : QObject(parent)
    , m_sessionController(nullptr)
{
    m_moveTimer.setInterval(MOVE_COALESCE_INTERVAL_MS);
    m_moveTimer.setSingleShot(true);
    connect(&m_moveTimer, &QTimer::timeout,
            this, &RemoteInputSender::flushPendingMove);
}

void RemoteInputSender::setSessionController(
        AdminSessionController *sessionController)
{
    m_moveTimer.stop();
    m_pendingMove.reset();
    m_sessionController = sessionController;
}

void RemoteInputSender::submitMouseInput(const RemoteMouseEvent &event)
{
    if (event.action == RemoteMouseAction::Move) {
        m_pendingMove = event;
        if (!m_moveTimer.isActive())
            m_moveTimer.start();
        return;
    }

    // Button/wheel packets contain current coordinates, so an older pending
    // move can be discarded without allowing it to run after the action.
    m_moveTimer.stop();
    m_pendingMove.reset();
    sendImmediately(event);
}

void RemoteInputSender::flushPendingMove()
{
    if (!m_pendingMove.has_value())
        return;

    const RemoteMouseEvent event = m_pendingMove.value();
    m_pendingMove.reset();
    sendImmediately(event);
}

void RemoteInputSender::sendImmediately(const RemoteMouseEvent &event)
{
    if (!m_sessionController) {
        qWarning() << "[RemoteInputSender] Chua co ADMIN session transport.";
        return;
    }

    const auto messageType = messageTypeForAction(event.action);
    if (!messageType.has_value()) {
        qWarning() << "[RemoteInputSender] Mouse action khong hop le.";
        return;
    }

    const auto payload = MouseProtocolCodec::encodePayload(
            messageType.value(), event);
    if (!payload.has_value()) {
        qWarning() << "[RemoteInputSender] Khong the ma hoa mouse payload.";
        return;
    }

    if (!m_sessionController->sendSessionMessage(
                messageType.value(), payload.value())) {
        qWarning() << "[RemoteInputSender] Khong the gui mouse message.";
    }
}
