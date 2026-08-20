// ============================================================
// ProtocolConstants.h
// Phase 1A.1 – Wire-protocol foundation: constants & MessageType
// ============================================================
// C++17 · fixed-width integer types (cstdint)
// ============================================================

#pragma once

#include <cstdint>

namespace Protocol {

// ------------------------------------------------------------
// Magic number – first 4 bytes of every frame ("RDTP")
// 0x52445450 = { 'R', 'D', 'T', 'P' }
// ------------------------------------------------------------
inline constexpr uint32_t PROTOCOL_MAGIC = 0x52445450U;

// ------------------------------------------------------------
// Wire-format version – bumped on breaking changes
// ------------------------------------------------------------
inline constexpr uint8_t PROTOCOL_VERSION = 1U;

// ------------------------------------------------------------
// Fixed protocol header size:
// magic(4) + version(1) + type(1) + flags(2) +
// payloadLength(4) + sessionId(8) + sequenceNumber(4)
// = 24 bytes
// ------------------------------------------------------------
inline constexpr uint32_t HEADER_SIZE = 24U;

// ------------------------------------------------------------
// Maximum payload length: 512 KiB
// ------------------------------------------------------------
inline constexpr uint32_t MAX_PAYLOAD_LENGTH = 524288U; // 512 * 1024

// ------------------------------------------------------------
// MessageType
// underlying type = uint8_t  →  max 256 distinct messages
// All values are explicit and non-overlapping.
// ------------------------------------------------------------
enum class MessageType : uint8_t {

  // ── Session negotiation ──────────────────────────────────
  REGISTER_HOST = 0x01,   ///< Host registers itself with the relay
  REGISTER_ACK = 0x02,    ///< Relay acknowledges the registration
  CONNECT_REQUEST = 0x03, ///< Viewer requests connection to a host
  SESSION_REQUEST = 0x04, ///< Relay forwards connection request to host
  SESSION_ACCEPT = 0x05,  ///< Host accepts the session
  SESSION_REJECT = 0x06,  ///< Host rejects the session
  CONNECT_RESULT = 0x07,  ///< Relay delivers final result to viewer

  // ── Media stream ────────────────────────────────────────
  SCREEN_FRAME = 0x10, ///< Encoded video / screen-capture frame

  // ── Remote input ────────────────────────────────────────
  MOUSE_MOVE = 0x20,        ///< Pointer movement (absolute or relative)
  MOUSE_BUTTON_DOWN = 0x21, ///< Mouse button pressed
  MOUSE_BUTTON_UP = 0x22,   ///< Mouse button released
  MOUSE_WHEEL = 0x23,       ///< Scroll wheel delta

  KEY_PRESS = 0x30,   ///< Keyboard key pressed
  KEY_RELEASE = 0x31, ///< Keyboard key released

  // ── Connection maintenance ───────────────────────────────
  HEARTBEAT = 0x40, ///< Periodic keep-alive (no reply expected)
  PING = 0x41,      ///< Round-trip latency probe (expects PONG)
  PONG = 0x42,      ///< Reply to PING

  // ── Diagnostics & control ────────────────────────────────
  TELEMETRY = 0x50,  ///< Bandwidth / frame-rate statistics payload
  DISCONNECT = 0x60, ///< Graceful session teardown
  ERROR = 0x70,      ///< Protocol-level error notification
};

} // namespace Protocol
