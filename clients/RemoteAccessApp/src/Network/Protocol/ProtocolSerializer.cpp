#include "ProtocolSerializer.h"

namespace Protocol {

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

QByteArray ProtocolSerializer::serializeHeader(const ProtocolHeader &header)
{
    QByteArray buf;
    buf.reserve(static_cast<int>(HEADER_SIZE));

    appendUInt32BE(buf, header.magic);
    appendUInt8(buf, header.version);
    appendUInt8(buf, static_cast<uint8_t>(header.type));
    appendUInt16BE(buf, header.flags);
    appendUInt32BE(buf, header.payloadLength);
    appendUInt64BE(buf, header.sessionId);
    appendUInt32BE(buf, header.sequenceNumber);

    Q_ASSERT(buf.size() == static_cast<int>(HEADER_SIZE));
    return buf;
}

std::optional<ProtocolHeader> ProtocolSerializer::deserializeHeader(const QByteArray &data)
{
    if (data.size() != static_cast<int>(HEADER_SIZE)) {
        return std::nullopt;
    }

    const uint8_t rawType = readUInt8(data, 5);
    const MessageType type = static_cast<MessageType>(rawType);

    ProtocolHeader header(type);

    header.magic = readUInt32BE(data, 0);
    header.version = readUInt8(data, 4);
    header.flags = readUInt16BE(data, 6);
    header.payloadLength = readUInt32BE(data, 8);
    header.sessionId = readUInt64BE(data, 12);
    header.sequenceNumber = readUInt32BE(data, 20);

    return header;
}

}
