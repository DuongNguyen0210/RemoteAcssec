// ============================================================
// ProtocolSerializer.h
// Phase 1A.2B - Header serialization (wire format)
// ============================================================
// Responsibility: serialize a ProtocolHeader into exactly
// HEADER_SIZE (24) bytes following RDTP wire format.
//
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
// Out of scope (Phase 1A.2B):
//   - deserialization
//   - packet framing / parser
//   - TCP / Session / Relay
// ============================================================

#pragma once

#include <QByteArray>
#include "ProtocolHeader.h"

namespace Protocol {

class ProtocolSerializer
{
public:
    // Serialize header into exactly HEADER_SIZE bytes (big-endian).
    // The caller is responsible for setting header fields before calling.
    static QByteArray serializeHeader(const ProtocolHeader &header);
};

} // namespace Protocol