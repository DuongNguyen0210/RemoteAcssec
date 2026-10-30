#ifndef SCREENFRAMEREASSEMBLER_H
#define SCREENFRAMEREASSEMBLER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QVector>
#include <QtGlobal>
#include <cstdint>
#include <optional>

namespace Protocol {

struct ReassembledFrame
{
    quint32 frameId = 0;
    QByteArray data;
};

/**
 * @brief Assembles incoming RDTP SCREEN_FRAME chunks into complete JPEG frames.
 *
 * Wire contract (all uint32, Big Endian):
 *   offset  0 : frameId
 *   offset  4 : chunkIndex  (0-based)
 *   offset  8 : chunkCount
 *   offset 12 : totalFrameSize
 *   offset 16 : chunk JPEG bytes
 *
 * Thread safety: Designed to be called on a single thread (e.g., event loop).
 * Memory safety: Bounds in-flight frames to MAX_IN_FLIGHT_FRAMES to prevent memory exhaustion.
 */
class ScreenFrameReassembler
{
public:
    static constexpr int METADATA_SIZE = 16;
    static constexpr int MAX_IN_FLIGHT_FRAMES = 4;

    ScreenFrameReassembler();
    ~ScreenFrameReassembler() = default;

    /**
     * @brief Feed one SCREEN_FRAME raw payload (metadata + chunk data).
     * @param payload raw payload bytes from an RDTP SCREEN_FRAME message.
     * @return ReassembledFrame if this chunk completed a frame; std::nullopt otherwise.
     */
    std::optional<ReassembledFrame> feed(const QByteArray &payload);

    /**
     * @brief Clears all pending in-flight frame buffers.
     */
    void clear();

    /**
     * @brief Returns current number of incomplete frames in memory.
     */
    int inFlightCount() const;

private:
    struct FrameState
    {
        quint32 frameId = 0;
        quint32 chunkCount = 0;
        quint32 totalFrameSize = 0;
        quint32 receivedChunks = 0;
        QVector<QByteArray> chunks;
    };

    void evictOldest();

    QHash<quint32, FrameState> m_frames;
    QList<quint32> m_arrivalOrder;
};

} // namespace Protocol

#endif // SCREENFRAMEREASSEMBLER_H
