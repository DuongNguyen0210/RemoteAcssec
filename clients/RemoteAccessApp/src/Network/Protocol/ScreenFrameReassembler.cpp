#include "ScreenFrameReassembler.h"
#include <QtEndian>
#include <QDebug>

namespace Protocol {

ScreenFrameReassembler::ScreenFrameReassembler()
{
}

std::optional<ReassembledFrame> ScreenFrameReassembler::feed(const QByteArray &payload)
{
    if (payload.size() < METADATA_SIZE) {
        return std::nullopt;
    }

    const auto *metadata = reinterpret_cast<const uchar *>(payload.constData());

    const quint32 frameId        = qFromBigEndian<quint32>(metadata);
    const quint32 chunkIndex     = qFromBigEndian<quint32>(metadata + 4);
    const quint32 chunkCount     = qFromBigEndian<quint32>(metadata + 8);
    const quint32 totalFrameSize = qFromBigEndian<quint32>(metadata + 12);

    if (chunkCount == 0 || chunkIndex >= chunkCount || totalFrameSize == 0) {
        return std::nullopt;
    }

    const int chunkDataLen = payload.size() - METADATA_SIZE;
    if (chunkDataLen <= 0) {
        return std::nullopt;
    }

    auto it = m_frames.find(frameId);
    if (it == m_frames.end()) {
        if (m_frames.size() >= MAX_IN_FLIGHT_FRAMES) {
            evictOldest();
        }

        FrameState state;
        state.frameId = frameId;
        state.chunkCount = chunkCount;
        state.totalFrameSize = totalFrameSize;
        state.receivedChunks = 0;
        state.chunks.resize(static_cast<int>(chunkCount));

        it = m_frames.insert(frameId, state);
        m_arrivalOrder.append(frameId);
    } else {
        // Validate consistency with existing state
        if (it.value().chunkCount != chunkCount || it.value().totalFrameSize != totalFrameSize) {
            m_frames.remove(frameId);
            m_arrivalOrder.removeAll(frameId);
            return std::nullopt;
        }
    }

    FrameState &state = it.value();

    // Check duplicate
    if (!state.chunks[static_cast<int>(chunkIndex)].isEmpty()) {
        return std::nullopt;
    }

    state.chunks[static_cast<int>(chunkIndex)] = payload.mid(METADATA_SIZE, chunkDataLen);
    state.receivedChunks++;

    if (state.receivedChunks < state.chunkCount) {
        return std::nullopt;
    }

    // Frame complete: reconstruct JPEG
    QByteArray completeFrame;
    completeFrame.reserve(static_cast<int>(totalFrameSize));

    for (const QByteArray &chunk : state.chunks) {
        if (chunk.isEmpty()) {
            m_frames.remove(frameId);
            m_arrivalOrder.removeAll(frameId);
            return std::nullopt;
        }
        completeFrame.append(chunk);
    }

    m_frames.remove(frameId);
    m_arrivalOrder.removeAll(frameId);

    if (static_cast<quint32>(completeFrame.size()) != totalFrameSize) {
        return std::nullopt;
    }

    return ReassembledFrame{frameId, completeFrame};
}

void ScreenFrameReassembler::clear()
{
    m_frames.clear();
    m_arrivalOrder.clear();
}

int ScreenFrameReassembler::inFlightCount() const
{
    return m_frames.size();
}

void ScreenFrameReassembler::evictOldest()
{
    if (m_arrivalOrder.isEmpty()) {
        if (!m_frames.isEmpty()) {
            m_frames.remove(m_frames.keyBegin().operator*());
        }
        return;
    }

    const quint32 oldestId = m_arrivalOrder.takeFirst();
    m_frames.remove(oldestId);
}

} // namespace Protocol
