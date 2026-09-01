#include "ScreenFrameReassembler.h"

#include "Network/Protocol/protocolconstants.h"

#include <QDebug>

namespace {

constexpr uint32_t MAX_CHUNK_DATA_BYTES =
        Protocol::MAX_PAYLOAD_LENGTH
        - static_cast<uint32_t>(ScreenFrameReassembler::METADATA_SIZE);

}

uint32_t ScreenFrameReassembler::readUInt32BE(const QByteArray &payload, int offset)
{
    return (static_cast<uint32_t>(static_cast<uint8_t>(payload.at(offset))) << 24)
            | (static_cast<uint32_t>(static_cast<uint8_t>(payload.at(offset + 1))) << 16)
            | (static_cast<uint32_t>(static_cast<uint8_t>(payload.at(offset + 2))) << 8)
            | static_cast<uint32_t>(static_cast<uint8_t>(payload.at(offset + 3)));
}

bool ScreenFrameReassembler::isNewerFrameId(uint32_t candidate, uint32_t reference)
{
    const uint32_t distance = candidate - reference;
    return distance != 0 && distance < 0x80000000U;
}

std::optional<ScreenFrameReassembler::CompleteFrame>
ScreenFrameReassembler::addChunk(const QByteArray &payload,
                                 uint32_t expectedChunkIndex)
{
    if (payload.size() < METADATA_SIZE) {
        qWarning() << "[ScreenFrameReassembler] Bo qua payload SCREEN_FRAME qua ngan.";
        return std::nullopt;
    }

    const uint32_t frameId = readUInt32BE(payload, 0);
    const uint32_t chunkIndex = readUInt32BE(payload, 4);
    const uint32_t chunkCount = readUInt32BE(payload, 8);
    const uint32_t totalFrameSize = readUInt32BE(payload, 12);

    if (chunkCount == 0 || chunkCount > MAX_CHUNK_COUNT) {
        qWarning() << "[ScreenFrameReassembler] chunkCount khong hop le, frameId="
                   << frameId << "chunkCount=" << chunkCount;
        removeFrame(frameId);
        return std::nullopt;
    }
    if (chunkIndex >= chunkCount || chunkIndex != expectedChunkIndex) {
        qWarning() << "[ScreenFrameReassembler] chunkIndex khong hop le, frameId="
                   << frameId << "chunkIndex=" << chunkIndex
                   << "sequenceNumber=" << expectedChunkIndex;
        removeFrame(frameId);
        return std::nullopt;
    }
    if (totalFrameSize == 0 || totalFrameSize > MAX_FRAME_SIZE_BYTES) {
        qWarning() << "[ScreenFrameReassembler] totalFrameSize khong hop le, frameId="
                   << frameId << "totalFrameSize=" << totalFrameSize;
        removeFrame(frameId);
        return std::nullopt;
    }

    const QByteArray chunkData = payload.mid(METADATA_SIZE);
    if (chunkData.isEmpty()
            || static_cast<uint32_t>(chunkData.size()) > MAX_CHUNK_DATA_BYTES
            || static_cast<uint64_t>(chunkData.size()) > totalFrameSize) {
        qWarning() << "[ScreenFrameReassembler] Kich thuoc chunk khong hop le, frameId="
                   << frameId << "chunkBytes=" << chunkData.size();
        removeFrame(frameId);
        return std::nullopt;
    }

    auto frameIt = m_frames.find(frameId);
    if (frameIt == m_frames.end()) {
        if (m_latestFrameId.has_value()) {
            if (frameId == m_latestFrameId.value()
                    || !isNewerFrameId(frameId, m_latestFrameId.value())) {
                qWarning() << "[ScreenFrameReassembler] Bo qua chunk cua frame cu, frameId="
                           << frameId;
                return std::nullopt;
            }
        }

        m_latestFrameId = frameId;
        if (m_frames.size() >= MAX_INCOMPLETE_FRAMES && !m_frameOrder.isEmpty()) {
            const uint32_t staleFrameId = m_frameOrder.takeFirst();
            m_frames.remove(staleFrameId);
            qWarning() << "[ScreenFrameReassembler] Loai frame chua hoan tat cu, frameId="
                       << staleFrameId;
        }

        m_frames.insert(frameId, FrameState(chunkCount, totalFrameSize));
        m_frameOrder.append(frameId);
        frameIt = m_frames.find(frameId);
    }

    FrameState &state = frameIt.value();
    if (state.chunkCount != chunkCount || state.totalFrameSize != totalFrameSize) {
        qWarning() << "[ScreenFrameReassembler] Metadata khong dong nhat, frameId="
                   << frameId;
        removeFrame(frameId);
        return std::nullopt;
    }

    const int chunkSlot = static_cast<int>(chunkIndex);
    if (state.received.at(chunkSlot)) {
        if (state.chunks.at(chunkSlot) != chunkData) {
            qWarning() << "[ScreenFrameReassembler] Chunk trung lap khong dong nhat, frameId="
                       << frameId << "chunkIndex=" << chunkIndex;
            removeFrame(frameId);
        }
        return std::nullopt;
    }

    const uint64_t newReceivedBytes = state.receivedBytes
            + static_cast<uint64_t>(chunkData.size());
    if (newReceivedBytes > state.totalFrameSize) {
        qWarning() << "[ScreenFrameReassembler] Tong du lieu vuot totalFrameSize, frameId="
                   << frameId;
        removeFrame(frameId);
        return std::nullopt;
    }

    state.chunks[chunkSlot] = chunkData;
    state.received[chunkSlot] = true;
    state.receivedBytes = newReceivedBytes;
    ++state.receivedChunkCount;

    if (state.receivedChunkCount < state.chunkCount)
        return std::nullopt;

    if (state.receivedBytes != state.totalFrameSize) {
        qWarning() << "[ScreenFrameReassembler] Kich thuoc frame hoan tat khong khop, frameId="
                   << frameId << "received=" << state.receivedBytes
                   << "expected=" << state.totalFrameSize;
        removeFrame(frameId);
        return std::nullopt;
    }

    if (m_lastCompletedFrameId.has_value()
            && !isNewerFrameId(frameId, m_lastCompletedFrameId.value())) {
        qWarning() << "[ScreenFrameReassembler] Bo qua frame hoan tat da cu, frameId="
                   << frameId;
        removeFrame(frameId);
        return std::nullopt;
    }

    QByteArray jpegBytes;
    jpegBytes.reserve(static_cast<int>(state.totalFrameSize));
    for (const QByteArray &chunk : state.chunks)
        jpegBytes.append(chunk);

    removeFrame(frameId);
    removeFramesOlderThan(frameId);
    m_lastCompletedFrameId = frameId;

    if (jpegBytes.size() != static_cast<int>(totalFrameSize)) {
        qWarning() << "[ScreenFrameReassembler] Kich thuoc JPEG sau khi ghep khong khop, frameId="
                   << frameId;
        return std::nullopt;
    }

    return CompleteFrame{frameId, jpegBytes};
}

void ScreenFrameReassembler::reset()
{
    m_frames.clear();
    m_frameOrder.clear();
    m_latestFrameId.reset();
    m_lastCompletedFrameId.reset();
}

void ScreenFrameReassembler::removeFrame(uint32_t frameId)
{
    m_frames.remove(frameId);
    m_frameOrder.removeAll(frameId);
}

void ScreenFrameReassembler::removeFramesOlderThan(uint32_t frameId)
{
    const QList<uint32_t> frameIds = m_frameOrder;
    for (uint32_t candidate : frameIds) {
        if (candidate != frameId && !isNewerFrameId(candidate, frameId))
            removeFrame(candidate);
    }
}
