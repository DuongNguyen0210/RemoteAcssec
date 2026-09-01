#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QVector>

#include <cstdint>
#include <optional>

class ScreenFrameReassembler
{
public:
    static constexpr int METADATA_SIZE = 16;
    static constexpr uint32_t MAX_CHUNK_COUNT = 64;
    static constexpr uint32_t MAX_FRAME_SIZE_BYTES = 16U * 1024U * 1024U;
    static constexpr int MAX_INCOMPLETE_FRAMES = 2;

    struct CompleteFrame {
        uint32_t frameId;
        QByteArray jpegBytes;
    };

    std::optional<CompleteFrame> addChunk(const QByteArray &payload,
                                          uint32_t expectedChunkIndex);
    void reset();

private:
    struct FrameState {
        FrameState(uint32_t expectedChunkCount, uint32_t expectedFrameSize)
            : chunkCount(expectedChunkCount)
            , totalFrameSize(expectedFrameSize)
            , chunks(static_cast<int>(expectedChunkCount))
            , received(static_cast<int>(expectedChunkCount), false)
            , receivedChunkCount(0)
            , receivedBytes(0)
        {
        }

        uint32_t chunkCount;
        uint32_t totalFrameSize;
        QVector<QByteArray> chunks;
        QVector<bool> received;
        uint32_t receivedChunkCount;
        uint64_t receivedBytes;
    };

    static uint32_t readUInt32BE(const QByteArray &payload, int offset);
    static bool isNewerFrameId(uint32_t candidate, uint32_t reference);
    void removeFrame(uint32_t frameId);
    void removeFramesOlderThan(uint32_t frameId);

    QHash<uint32_t, FrameState> m_frames;
    QList<uint32_t> m_frameOrder;
    std::optional<uint32_t> m_latestFrameId;
    std::optional<uint32_t> m_lastCompletedFrameId;
};
