#include "ScreenFramePacketizer.h"

#include "Protocolconstants.h"
#include "Protocolheader.h"
#include "ProtocolSerializer.h"

#include <QDebug>
#include <cmath>     // std::ceil — not strictly needed but kept for clarity

// ---------------------------------------------------------------------------
// maxChunkData()
// ---------------------------------------------------------------------------

constexpr uint32_t ScreenFramePacketizer::maxChunkData()
{
    return Protocol::MAX_PAYLOAD_LENGTH
           - static_cast<uint32_t>(SCREEN_FRAME_METADATA_SIZE);
}

// ---------------------------------------------------------------------------
// appendUInt32BE() — local helper mirroring the serializer's private helpers.
// We cannot call the serializer's static helpers (they are file-static in
// ProtocolSerializer.cpp), so we replicate the trivial 4-byte BE write here.
// ---------------------------------------------------------------------------

static void appendU32BE(QByteArray &buf, uint32_t value)
{
    buf.append(static_cast<char>((value >> 24) & 0xFF));
    buf.append(static_cast<char>((value >> 16) & 0xFF));
    buf.append(static_cast<char>((value >>  8) & 0xFF));
    buf.append(static_cast<char>((value >>  0) & 0xFF));
}

// ---------------------------------------------------------------------------
// packetize()
// ---------------------------------------------------------------------------

QList<QByteArray> ScreenFramePacketizer::packetize(const QByteArray &encodedFrame,
                                                    uint32_t frameId,
                                                    uint64_t sessionId)
{
    // --- guard: empty frame -------------------------------------------------
    if (encodedFrame.isEmpty()) {
        qWarning() << "[ScreenFramePacketizer] packetize() called with empty frame.";
        return {};
    }

    const uint32_t frameSize    = static_cast<uint32_t>(encodedFrame.size());
    const uint32_t chunkDataMax = maxChunkData();

    // --- guard: degenerate MAX_CHUNK_DATA (should never happen with current constants)
    if (chunkDataMax == 0) {
        qWarning() << "[ScreenFramePacketizer] maxChunkData() == 0, cannot packetize.";
        return {};
    }

    // --- calculate chunk count ----------------------------------------------
    // Use integer ceiling: chunkCount = (frameSize + chunkDataMax - 1) / chunkDataMax
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

        // Paranoia: enforce MAX_PAYLOAD_LENGTH before emitting.
        if (payloadLen > Protocol::MAX_PAYLOAD_LENGTH) {
            qCritical() << "[ScreenFramePacketizer] BUG: payloadLen" << payloadLen
                        << "> MAX_PAYLOAD_LENGTH" << Protocol::MAX_PAYLOAD_LENGTH
                        << "at chunk" << i << "— aborting.";
            return {};
        }

        // --- build RDTP header ----------------------------------------------
        Protocol::ProtocolHeader header(Protocol::MessageType::SCREEN_FRAME);
        header.payloadLength  = payloadLen;
        header.sessionId      = sessionId;
        header.sequenceNumber = i;   // repurpose sequenceNumber as chunk index
                                     // within this call; the streaming layer
                                     // may override this field later.

        QByteArray packet = Protocol::ProtocolSerializer::serializeHeader(header);

        // --- build SCREEN_FRAME metadata prefix (16 bytes, Big Endian) ------
        appendU32BE(packet, frameId);        // offset  0: frameId
        appendU32BE(packet, i);              // offset  4: chunkIndex (0-based)
        appendU32BE(packet, chunkCount);     // offset  8: chunkCount
        appendU32BE(packet, frameSize);      // offset 12: totalFrameSize

        // --- append JPEG chunk data -----------------------------------------
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
