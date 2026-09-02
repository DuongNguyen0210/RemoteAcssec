#pragma once

#include <QByteArray>

#include <optional>

#include "Core/Services/RemoteControl/Input/RemoteMouseEvent.h"
#include "protocolconstants.h"

class MouseProtocolCodec
{
public:
    static std::optional<QByteArray> encodePayload(
            Protocol::MessageType type, const RemoteMouseEvent &event);

    static std::optional<RemoteMouseEvent> decodePayload(
            Protocol::MessageType type, const QByteArray &payload);
};
