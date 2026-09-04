#pragma once

#include <cstdint>

namespace Protocol {

inline constexpr uint32_t PROTOCOL_MAGIC = 0x52445450U;
inline constexpr uint8_t PROTOCOL_VERSION = 1U;
inline constexpr uint32_t HEADER_SIZE = 24U;
inline constexpr uint32_t MAX_PAYLOAD_LENGTH = 524288U;

enum class MessageType : uint8_t {

    REGISTER_HOST = 0x01,   ///< Host registers itself with the relay
    REGISTER_ACK = 0x02,    ///< Relay acknowledges the registration
    CONNECT_REQUEST = 0x03, ///< Viewer requests connection to a host
    SESSION_REQUEST = 0x04, ///< Relay forwards connection request to host
    SESSION_ACCEPT = 0x05,  ///< Host accepts the session
    SESSION_REJECT = 0x06,  ///< Host rejects the session
    CONNECT_RESULT = 0x07,  ///< Relay delivers final result to viewer

    SCREEN_FRAME = 0x10, ///< Encoded video / screen-capture frame

    MOUSE_MOVE = 0x20,        ///< Pointer movement (absolute or relative)
    MOUSE_BUTTON_DOWN = 0x21, ///< Mouse button pressed
    MOUSE_BUTTON_UP = 0x22,   ///< Mouse button released
    MOUSE_WHEEL = 0x23,       ///< Scroll wheel delta

    KEY_PRESS = 0x30,   ///< Keyboard key pressed
    KEY_RELEASE = 0x31, ///< Keyboard key released

    HEARTBEAT = 0x40, ///< Periodic keep-alive (no reply expected)
    PING = 0x41,      ///< Round-trip latency probe (expects PONG)
    PONG = 0x42,      ///< Reply to PING

    TELEMETRY = 0x50,  ///< Bandwidth / frame-rate statistics payload
    DISCONNECT = 0x60, ///< Graceful session teardown
    ERROR = 0x70,      ///< Protocol-level error notification
};

} // namespace Protocol
