// ============================================================
// ProtocolSerializer.cpp
// Phase 1A.2B - Header serialization   (wire format)
// Phase 1A.2C - Header deserialization (wire format)
// ============================================================

#include "ProtocolSerializer.h"

namespace Protocol {

// ============================================================
// Internal write helpers (Phase 1A.2B)
// Append integer fields in big-endian order.
// These write each byte individually - no struct memory copy.
// ============================================================

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

// ============================================================
// Internal read helpers (Phase 1A.2C)
// Read integer fields from big-endian byte stream.
// Cast each byte to uint8_t first to prevent sign extension
// on platforms where char is signed (bytes >= 0x80).
// ============================================================

static uint8_t readUInt8(const QByteArray &buf, int offset)
{
    return static_cast<uint8_t>(buf.at(offset));
}

static uint16_t readUInt16BE(const QByteArray &buf, int offset)
{
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(static_cast<uint8_t>(buf.at(offset    ))) << 8) |
        (static_cast<uint16_t>(static_cast<uint8_t>(buf.at(offset + 1))) << 0)
    );
}

static uint32_t readUInt32BE(const QByteArray &buf, int offset)
{
    return
        (static_cast<uint32_t>(static_cast<uint8_t>(buf.at(offset    ))) << 24) |
        (static_cast<uint32_t>(static_cast<uint8_t>(buf.at(offset + 1))) << 16) |
        (static_cast<uint32_t>(static_cast<uint8_t>(buf.at(offset + 2))) <<  8) |
        (static_cast<uint32_t>(static_cast<uint8_t>(buf.at(offset + 3))) <<  0);
}

static uint64_t readUInt64BE(const QByteArray &buf, int offset)
{
    return
        (static_cast<uint64_t>(static_cast<uint8_t>(buf.at(offset    ))) << 56) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf.at(offset + 1))) << 48) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf.at(offset + 2))) << 40) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf.at(offset + 3))) << 32) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf.at(offset + 4))) << 24) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf.at(offset + 5))) << 16) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf.at(offset + 6))) <<  8) |
        (static_cast<uint64_t>(static_cast<uint8_t>(buf.at(offset + 7))) <<  0);
}

// ============================================================
// Phase 1A.2B - serializeHeader
// ============================================================

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

// ============================================================
// Phase 1A.2C - deserializeHeader
// ============================================================

std::optional<ProtocolHeader>
ProtocolSerializer::deserializeHeader(const QByteArray &data)
{
    // Size validation - the only validation in scope for Phase 1A.2C.
    // Stream fragmentation and reassembly are out of scope.
    if (data.size() != static_cast<int>(HEADER_SIZE)) {
        return std::nullopt;
    }

    // Offset  5    : type (MessageType : uint8_t)
    // Read type first because ProtocolHeader constructor requires it.
    const uint8_t rawType = readUInt8(data, 5);
    const MessageType type = static_cast<MessageType>(rawType);

    // Construct with required MessageType; defaults will be overwritten below.
    ProtocolHeader header(type);

    // Offset  0-3  : magic (uint32_t, 4 bytes)
    header.magic          = readUInt32BE(data,  0);

    // Offset  4    : version (uint8_t, 1 byte)
    header.version        = readUInt8   (data,  4);

    // type already set via constructor (offset 5)

    // Offset  6-7  : flags (uint16_t, 2 bytes)
    header.flags          = readUInt16BE(data,  6);

    // Offset  8-11 : payloadLength (uint32_t, 4 bytes)
    header.payloadLength  = readUInt32BE(data,  8);

    // Offset 12-19 : sessionId (uint64_t, 8 bytes)
    header.sessionId      = readUInt64BE(data, 12);

    // Offset 20-23 : sequenceNumber (uint32_t, 4 bytes)
    header.sequenceNumber = readUInt32BE(data, 20);

    return header;
}

} // namespace Protocol