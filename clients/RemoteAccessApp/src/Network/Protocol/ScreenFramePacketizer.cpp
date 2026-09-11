#include "ScreenFramePacketizer.h"

#include "ProtocolConstants.h"
#include "ProtocolHeader.h"
#include "ProtocolSerializer.h"

#include <QDebug>

static void appendU32BE(QByteArray &buf, uint32_t value)
{
    buf.append(static_cast<char>((value >> 24) & 0xFF));
    buf.append(static_cast<char>((value >> 16) & 0xFF));
    buf.append(static_cast<char>((value >>  8) & 0xFF));
    buf.append(static_cast<char>((value >>  0) & 0xFF));
}

QList<QByteArray> ScreenFramePacketizer::packetize(const QByteArray &encodedFrame,
                                                    uint32_t frameId,
                                                    uint64_t sessionId)
{
    if (encodedFrame.isEmpty()) {
        qWarning() << "[ScreenFramePacketizer] packetize() called with empty frame.";
        return {};
    }

    const uint32_t frameSize    = static_cast<uint32_t>(encodedFrame.size());
    const uint32_t chunkDataMax = maxChunkData();

    if (chunkDataMax == 0) {
        qWarning() << "[ScreenFramePacketizer] maxChunkData() == 0, cannot packetize.";
        return {};
    }

    const uint32_t chunkCount = (frameSize + chunkDataMax - 1) / chunkDataMax;

    if (chunkCount == 0) {
        qWarning() << "[ScreenFramePacketizer] chunkCount calculated as 0.";
        return {};
    }

    QList<QByteArray> packets;
    packets.reserve(static_cast<int>(chunkCount));

    for (uint32_t i = 0; i < chunkCount; ++i) {
        const uint32_t offset     = i * chunkDataMax;
        const uint32_t remaining  = frameSize - offset;
        const uint32_t chunkBytes = (remaining < chunkDataMax) ? remaining : chunkDataMax;
        const uint32_t payloadLen = static_cast<uint32_t>(SCREEN_FRAME_METADATA_SIZE) + chunkBytes;

        if (payloadLen > Protocol::MAX_PAYLOAD_LENGTH) {
            qCritical() << "[ScreenFramePacketizer] BUG: payloadLen" << payloadLen
                        << "> MAX_PAYLOAD_LENGTH" << Protocol::MAX_PAYLOAD_LENGTH
                        << "at chunk" << i;
            return {};
        }

        Protocol::ProtocolHeader header(Protocol::MessageType::SCREEN_FRAME);
        header.payloadLength  = payloadLen;
        header.sessionId      = sessionId;
        header.sequenceNumber = i;

        QByteArray packet = Protocol::ProtocolSerializer::serializeHeader(header);

        appendU32BE(packet, frameId);
        appendU32BE(packet, i);
        appendU32BE(packet, chunkCount);
        appendU32BE(packet, frameSize);

        packet.append(encodedFrame.constData() + offset,
                      static_cast<int>(chunkBytes));

        packets.append(packet);
    }

    qDebug() << "[ScreenFramePacketizer] frameId=" << frameId
             << "sessionId=" << sessionId
             << "frameBytes=" << frameSize
             << "chunkCount=" << chunkCount
             << "maxChunkData=" << chunkDataMax;

    return packets;
}
