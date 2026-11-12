#include "ScreenFrameReassembler.h"
#include <QtEndian>

namespace Protocol {
ScreenFrameReassembler::ScreenFrameReassembler() { m_clock.start(); }

bool ScreenFrameReassembler::isNewer(quint32 candidate, quint32 reference)
{
    const quint32 distance = candidate - reference;
    return distance != 0 && distance < 0x80000000u;
}

std::optional<ReassembledFrame> ScreenFrameReassembler::feed(const QByteArray &payload)
{
    if (payload.size() < METADATA_SIZE) return std::nullopt;
    const auto *data = reinterpret_cast<const uchar *>(payload.constData());
    const quint32 id = qFromBigEndian<quint32>(data);
    const quint32 index = qFromBigEndian<quint32>(data + 4);
    const quint32 count = qFromBigEndian<quint32>(data + 8);
    const quint32 size = qFromBigEndian<quint32>(data + 12);
    const QByteArray chunk = payload.mid(METADATA_SIZE);
    if (m_hasCompleted && !isNewer(id, m_latestCompleted)) return std::nullopt;
    if (!count || count > MAX_CHUNKS || index >= count || !size || size > MAX_FRAME_BYTES
            || count > size || chunk.isEmpty() || quint64(chunk.size()) > size) return std::nullopt;
    prune();
    auto it = m_frames.find(id);
    if (it == m_frames.end()) {
        if (m_frames.size() >= MAX_IN_FLIGHT_FRAMES) evictOldest();
        if (m_reservedBytes + size > MAX_IN_FLIGHT_BYTES) return std::nullopt;
        FrameState state;
        state.frameId = id; state.chunkCount = count; state.totalFrameSize = size;
        state.createdAt = m_clock.elapsed(); state.chunks.resize(int(count));
        it = m_frames.insert(id, state); m_arrivalOrder.append(id); m_reservedBytes += size;
    }
    FrameState &frame = it.value();
    if (frame.chunkCount != count || frame.totalFrameSize != size) {
        removeFrame(id); return std::nullopt;
    }
    if (!frame.chunks[int(index)].isEmpty()) return std::nullopt;
    if (quint64(frame.receivedBytes) + quint64(chunk.size()) > size) {
        removeFrame(id); return std::nullopt;
    }
    frame.chunks[int(index)] = chunk;
    frame.receivedBytes += quint32(chunk.size());
    if (++frame.receivedChunks != count) return std::nullopt;
    if (frame.receivedBytes != size) { removeFrame(id); return std::nullopt; }
    QByteArray complete; complete.reserve(int(size));
    for (const auto &part : frame.chunks) complete.append(part);
    removeFrame(id);
    m_hasCompleted = true; m_latestCompleted = id;
    const auto ids = m_frames.keys();
    for (auto old : ids) if (!isNewer(old, id)) removeFrame(old);
    return ReassembledFrame{id, complete};
}

void ScreenFrameReassembler::removeFrame(quint32 id)
{
    auto it = m_frames.find(id);
    if (it == m_frames.end()) return;
    m_reservedBytes -= it->totalFrameSize;
    m_frames.erase(it); m_arrivalOrder.removeAll(id);
}
void ScreenFrameReassembler::prune()
{
    const auto ids = m_frames.keys();
    for (auto id : ids) {
        auto it = m_frames.constFind(id);
        if (it != m_frames.cend() && m_clock.elapsed() - it->createdAt >= FRAME_TTL_MS) removeFrame(id);
    }
}
void ScreenFrameReassembler::clear()
{
    m_frames.clear(); m_arrivalOrder.clear(); m_reservedBytes = 0; m_hasCompleted = false;
}
int ScreenFrameReassembler::inFlightCount() const { return m_frames.size(); }
void ScreenFrameReassembler::evictOldest() { if (!m_arrivalOrder.isEmpty()) removeFrame(m_arrivalOrder.first()); }
}
