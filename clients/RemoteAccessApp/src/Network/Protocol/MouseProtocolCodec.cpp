#include "MouseProtocolCodec.h"

#include <cstdint>

namespace {

void appendUInt16BE(QByteArray &payload, quint16 value)
{
    payload.append(static_cast<char>((value >> 8) & 0xFF));
    payload.append(static_cast<char>(value & 0xFF));
}

void appendUInt32BE(QByteArray &payload, quint32 value)
{
    payload.append(static_cast<char>((value >> 24) & 0xFF));
    payload.append(static_cast<char>((value >> 16) & 0xFF));
    payload.append(static_cast<char>((value >> 8) & 0xFF));
    payload.append(static_cast<char>(value & 0xFF));
}

quint16 readUInt16BE(const QByteArray &payload, int offset)
{
    return static_cast<quint16>(
            (static_cast<quint16>(static_cast<quint8>(payload.at(offset))) << 8)
            | static_cast<quint16>(static_cast<quint8>(payload.at(offset + 1))));
}

quint32 readUInt32BE(const QByteArray &payload, int offset)
{
    return (static_cast<quint32>(static_cast<quint8>(payload.at(offset))) << 24)
            | (static_cast<quint32>(static_cast<quint8>(payload.at(offset + 1))) << 16)
            | (static_cast<quint32>(static_cast<quint8>(payload.at(offset + 2))) << 8)
            | static_cast<quint32>(static_cast<quint8>(payload.at(offset + 3)));
}

bool isSupportedButton(RemoteMouseButton button)
{
    return button == RemoteMouseButton::Left
            || button == RemoteMouseButton::Right
            || button == RemoteMouseButton::Middle;
}

std::optional<RemoteMouseButton> decodeButton(quint8 value)
{
    switch (value) {
    case 1:
        return RemoteMouseButton::Left;
    case 2:
        return RemoteMouseButton::Right;
    case 3:
        return RemoteMouseButton::Middle;
    default:
        return std::nullopt;
    }
}

}

std::optional<QByteArray> MouseProtocolCodec::encodePayload(
        Protocol::MessageType type, const RemoteMouseEvent &event)
{
    QByteArray payload;

    switch (type) {
    case Protocol::MessageType::MOUSE_MOVE:
        if (event.action != RemoteMouseAction::Move)
            return std::nullopt;
        payload.reserve(4);
        appendUInt16BE(payload, event.normalizedX);
        appendUInt16BE(payload, event.normalizedY);
        return payload;

    case Protocol::MessageType::MOUSE_BUTTON_DOWN:
    case Protocol::MessageType::MOUSE_BUTTON_UP: {
        const RemoteMouseAction expectedAction =
                type == Protocol::MessageType::MOUSE_BUTTON_DOWN
                ? RemoteMouseAction::ButtonDown
                : RemoteMouseAction::ButtonUp;
        if (event.action != expectedAction || !isSupportedButton(event.button))
            return std::nullopt;
        payload.reserve(5);
        payload.append(static_cast<char>(event.button));
        appendUInt16BE(payload, event.normalizedX);
        appendUInt16BE(payload, event.normalizedY);
        return payload;
    }

    case Protocol::MessageType::MOUSE_WHEEL:
        if (event.action != RemoteMouseAction::Wheel)
            return std::nullopt;
        payload.reserve(8);
        appendUInt16BE(payload, event.normalizedX);
        appendUInt16BE(payload, event.normalizedY);
        appendUInt32BE(payload, static_cast<quint32>(event.wheelDelta));
        return payload;

    default:
        return std::nullopt;
    }
}

std::optional<RemoteMouseEvent> MouseProtocolCodec::decodePayload(
        Protocol::MessageType type, const QByteArray &payload)
{
    RemoteMouseEvent event;

    switch (type) {
    case Protocol::MessageType::MOUSE_MOVE:
        if (payload.size() != 4)
            return std::nullopt;
        event.action = RemoteMouseAction::Move;
        event.normalizedX = readUInt16BE(payload, 0);
        event.normalizedY = readUInt16BE(payload, 2);
        return event;

    case Protocol::MessageType::MOUSE_BUTTON_DOWN:
    case Protocol::MessageType::MOUSE_BUTTON_UP: {
        if (payload.size() != 5)
            return std::nullopt;
        const auto button = decodeButton(
                static_cast<quint8>(payload.at(0)));
        if (!button.has_value())
            return std::nullopt;
        event.action = type == Protocol::MessageType::MOUSE_BUTTON_DOWN
                ? RemoteMouseAction::ButtonDown
                : RemoteMouseAction::ButtonUp;
        event.button = button.value();
        event.normalizedX = readUInt16BE(payload, 1);
        event.normalizedY = readUInt16BE(payload, 3);
        return event;
    }

    case Protocol::MessageType::MOUSE_WHEEL: {
        if (payload.size() != 8)
            return std::nullopt;
        event.action = RemoteMouseAction::Wheel;
        event.normalizedX = readUInt16BE(payload, 0);
        event.normalizedY = readUInt16BE(payload, 2);
        const quint32 rawDelta = readUInt32BE(payload, 4);
        const qint64 signedDelta = rawDelta <= 0x7FFFFFFFU
                ? static_cast<qint64>(rawDelta)
                : static_cast<qint64>(rawDelta) - 0x100000000LL;
        event.wheelDelta = static_cast<qint32>(signedDelta);
        return event;
    }

    default:
        return std::nullopt;
    }
}
