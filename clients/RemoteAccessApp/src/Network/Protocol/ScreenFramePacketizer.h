#pragma once

#include <QByteArray>
#include <QList>
#include <cstdint>

#include "ProtocolConstants.h"

class ScreenFramePacketizer
{
public:
    static constexpr int SCREEN_FRAME_METADATA_SIZE = 16;

    static constexpr uint32_t maxChunkData()
    {
        return Protocol::MAX_PAYLOAD_LENGTH - static_cast<uint32_t>(SCREEN_FRAME_METADATA_SIZE);
    }

    static QList<QByteArray> packetize(const QByteArray &encodedFrame,
                                       uint32_t frameId,
                                       uint64_t sessionId);
};
