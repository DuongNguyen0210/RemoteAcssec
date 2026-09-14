#pragma once

#include <cstdint>

namespace Protocol {

inline constexpr uint32_t PROTOCOL_MAGIC = 0x52445450U;
inline constexpr uint8_t PROTOCOL_VERSION = 2U;
inline constexpr uint32_t HEADER_SIZE = 24U;
inline constexpr uint32_t MAX_PAYLOAD_LENGTH = 524288U;

enum class MessageType : uint8_t {
    REGISTER_HOST = 0x01,
    REGISTER_ACK = 0x02,
    CONNECT_REQUEST = 0x03,
    SESSION_REQUEST = 0x04,
    SESSION_ACCEPT = 0x05,
    SESSION_REJECT = 0x06,
    CONNECT_RESULT = 0x07,

    SCREEN_FRAME = 0x10,

    MOUSE_MOVE = 0x20,
    MOUSE_BUTTON_DOWN = 0x21,
    MOUSE_BUTTON_UP = 0x22,
    MOUSE_WHEEL = 0x23,

    KEY_PRESS = 0x30,
    KEY_RELEASE = 0x31,

    HEARTBEAT = 0x40,
    PING = 0x41,
    PONG = 0x42,

    TELEMETRY = 0x50,
    DISCONNECT = 0x60,
    ERROR = 0x70
};

}
