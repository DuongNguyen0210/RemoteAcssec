// ============================================================
// ProtocolHeader.h
// Phase 1A.2A – Wire-protocol header model
// ============================================================
// C++17 · fixed-width integer types (cstdint)
//
// NOTE: HEADER_SIZE = 24 is the wire-format size.
//       sizeof(ProtocolHeader) is NOT required to equal 24.
// ============================================================

#pragma once

#include <cstdint>
#include "ProtocolConstants.h"

namespace Protocol {

struct ProtocolHeader {
    uint32_t magic;
    uint8_t  version;
    MessageType type;
    uint16_t flags;
    uint32_t payloadLength;
    uint64_t sessionId;
    uint32_t sequenceNumber;

    explicit ProtocolHeader(MessageType messageType)
        : magic          { PROTOCOL_MAGIC }
        , version        { PROTOCOL_VERSION }
        , type           { messageType }
        , flags          { 0 }
        , payloadLength  { 0 }
        , sessionId      { 0 }
        , sequenceNumber { 0 }
    {}
};

} // namespace Protocol
