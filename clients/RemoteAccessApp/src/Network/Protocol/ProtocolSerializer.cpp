// ============================================================
// ProtocolSerializer.cpp
// Phase 1A.2B - Header serialization (wire format)
// ============================================================

#include "ProtocolSerializer.h"

namespace Protocol {

// ------------------------------------------------------------
// Internal helpers: append integer fields in big-endian order.
// These write each byte individually - no struct memory copy.
// ------------------------------------------------------------

static void appendUInt8(QByteArray &buf, uint8_t value)
{
    buf.append(static_cast<char>(value));
}

static void appendUInt16BE(QByteArray &buf, uint16_t value)
{
    buf.append(static_cast<char>((value >> 8) & 0xFF));
    buf.append(static_cast<char>((value >> 0) & 0xFF));
}

static void appendUInt32BE(QByteArray &buf, uint32_t value)
{
    buf.append(static_cast<char>((value >> 24) & 0xFF));
    buf.append(static_cast<char>((value >> 16) & 0xFF));
    buf.append(static_cast<char>((value >>  8) & 0xFF));
    buf.append(static_cast<char>((value >>  0) & 0xFF));
}

static void appendUInt64BE(QByteArray &buf, uint64_t value)
{
    buf.append(static_cast<char>((value >> 56) & 0xFF));
    buf.append(static_cast<char>((value >> 48) & 0xFF));
    buf.append(static_cast<char>((value >> 40) & 0xFF));
    buf.append(static_cast<char>((value >> 32) & 0xFF));
    buf.append(static_cast<char>((value >> 24) & 0xFF));
    buf.append(static_cast<char>((value >> 16) & 0xFF));
    buf.append(static_cast<char>((value >>  8) & 0xFF));
    buf.append(static_cast<char>((value >>  0) & 0xFF));
}

// ------------------------------------------------------------
// serializeHeader
// ------------------------------------------------------------

QByteArray ProtocolSerializer::serializeHeader(const ProtocolHeader &header)
{
    QByteArray buf;
    buf.reserve(static_cast<int>(HEADER_SIZE));

    // Offset  0-3  : magic (uint32_t, 4 bytes)
    appendUInt32BE(buf, header.magic);

    // Offset  4    : version (uint8_t, 1 byte)
    appendUInt8(buf, header.version);

    // Offset  5    : type (MessageType : uint8_t, 1 byte)
    appendUInt8(buf, static_cast<uint8_t>(header.type));

    // Offset  6-7  : flags (uint16_t, 2 bytes)
    appendUInt16BE(buf, header.flags);

    // Offset  8-11 : payloadLength (uint32_t, 4 bytes)
    appendUInt32BE(buf, header.payloadLength);

    // Offset 12-19 : sessionId (uint64_t, 8 bytes)
    appendUInt64BE(buf, header.sessionId);

    // Offset 20-23 : sequenceNumber (uint32_t, 4 bytes)
    appendUInt32BE(buf, header.sequenceNumber);

    Q_ASSERT(buf.size() == static_cast<int>(HEADER_SIZE));
    return buf;
}

} // namespace Protocol