#pragma once

#include <QByteArray>
#include <QList>
#include <cstdint>

// ---------------------------------------------------------------------------
// ScreenFramePacketizer
//
// GRASP responsibility: Information Expert for RDTP SCREEN_FRAME packetization.
//
// Takes an already-encoded JPEG QByteArray and produces one or more complete,
// ready-to-transmit RDTP message QByteArrays.
//
// This class knows NOTHING about:
//   - how the JPEG was captured  (ScreenCapture)
//   - how the JPEG was encoded   (ScreenEncoder)
//   - how packets are sent       (RelayClient / QTcpSocket)
//   - JWT / ApiClient / server address
//   - session routing (sessionId is set to 0 in this phase)
//   - timers / frame rate
//
// It depends only on protocolconstants.h / protocolheader.h /
// protocolserializer.h and Qt byte containers.
//
// ===========================================================================
// SCREEN_FRAME payload wire layout
// ===========================================================================
//
// Each RDTP packet for a SCREEN_FRAME consists of:
//
//   [RDTP Header]       24 bytes   — standard header, type=SCREEN_FRAME
//   [SCREEN_FRAME payload]         — layout below (total <= MAX_PAYLOAD_LENGTH)
//
// SCREEN_FRAME payload byte layout (all multi-byte fields Big Endian):
//
//   Offset  0 : frameId       uint32_t  4 bytes  — identifies this logical frame
//   Offset  4 : chunkIndex    uint32_t  4 bytes  — 0-based index of this chunk
//   Offset  8 : chunkCount    uint32_t  4 bytes  — total number of chunks
//   Offset 12 : totalFrameSize uint32_t 4 bytes  — full JPEG byte count
//   Offset 16 : [JPEG chunk data] variable bytes
//
// SCREEN_FRAME_METADATA_SIZE = 16 bytes
// MAX_CHUNK_DATA = MAX_PAYLOAD_LENGTH - SCREEN_FRAME_METADATA_SIZE
//               = 524288 - 16 = 524272 bytes
//
// Chunking rule:
//   - If encodedFrame.size() <= MAX_CHUNK_DATA  → 1 packet, chunkCount=1
//   - Otherwise → N packets where N = ceil(size / MAX_CHUNK_DATA)
//   - chunkIndex: 0 ... chunkCount-1  (zero-based)
//   - all chunks share the same frameId, chunkCount, totalFrameSize
//   - the last chunk may carry fewer bytes than MAX_CHUNK_DATA
// ===========================================================================

class ScreenFramePacketizer
{
public:
    // Size of the SCREEN_FRAME-specific metadata prefix inside the RDTP payload.
    static constexpr int SCREEN_FRAME_METADATA_SIZE = 16;  // 4 × uint32_t

    // Maximum JPEG bytes that fit in the data portion of one payload.
    // Computed from Protocol::MAX_PAYLOAD_LENGTH at construction-time to
    // allow the constant to remain the single source of truth.
    static constexpr uint32_t maxChunkData();

    // Packetize an encoded JPEG frame.
    //
    // Parameters:
    //   encodedFrame — JPEG QByteArray produced by ScreenEncoder.
    //   frameId      — caller-assigned identifier for this logical frame.
    //                  The future streaming layer should increment this per frame.
    //
    // Returns:
    //   A QList of complete RDTP message QByteArrays (header + payload).
    //   Returns an empty list if encodedFrame is empty or an internal error occurs.
    //
    // Every returned QByteArray satisfies:
    //   size == HEADER_SIZE + payloadLength
    //   payloadLength <= MAX_PAYLOAD_LENGTH
    //   type == SCREEN_FRAME
    //
    // Never throws. Never crashes.
    static QList<QByteArray> packetize(const QByteArray &encodedFrame,
                                       uint32_t frameId);
};
