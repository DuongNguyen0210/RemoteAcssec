// ============================================================
// ProtocolSerializer.h
// Phase 1A.2B - Header serialization   (wire format)
// Phase 1A.2C - Header deserialization (wire format)
// ============================================================
// Wire format (big-endian / network byte order):
//   Offset  Size  Field
//    0       4    magic
//    4       1    version
//    5       1    type
//    6       2    flags
//    8       4    payloadLength
//   12       8    sessionId
//   20       4    sequenceNumber
//
// Out of scope:
//   - packet framing / stream parser
//   - TCP / Session / Relay
// ============================================================

#pragma once

#include <optional>
#include <QByteArray>
#include "ProtocolHeader.h"

namespace Protocol {

class ProtocolSerializer
{
public:
    // Phase 1A.2B -------------------------------------------------
    // Serialize header into exactly HEADER_SIZE bytes (big-endian).
    // The caller is responsible for setting header fields before calling.
    static QByteArray serializeHeader(const ProtocolHeader &header);

    // Phase 1A.2C -------------------------------------------------
    // Deserialize exactly HEADER_SIZE bytes (big-endian) into a ProtocolHeader.
    // Returns std::nullopt if data.size() != HEADER_SIZE.
    // Does NOT validate magic, version, or MessageType range.
    // Does NOT handle TCP fragmentation or stream reassembly.
    static std::optional<ProtocolHeader>
    deserializeHeader(const QByteArray &data);
};

} // namespace Protocol