#pragma once

#include <cstdint>
#include "protocolconstants.h"

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

}
