package com.remotecontrol.relay.handler.screen;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.Map;

/**
 * ScreenFrameReassembler
 *
 * Responsibility: reassemble SCREEN_FRAME chunks into a complete JPEG byte[].
 *
 * Wire contract (all uint32, Big Endian):
 *   offset  0 : frameId
 *   offset  4 : chunkIndex  (0-based)
 *   offset  8 : chunkCount
 *   offset 12 : totalFrameSize
 *   offset 16 : chunk bytes
 *
 * One instance per client channel — no static/global state.
 */
public class ScreenFrameReassembler {

    /** Minimum payload bytes required to contain the metadata header. */
    public static final int METADATA_SIZE = 16;

    /**
     * Maximum number of in-flight frameIds tracked simultaneously.
     * If exceeded, the oldest entry is evicted to bound memory usage.
     */
    private static final int MAX_IN_FLIGHT_FRAMES = 4;

    // -----------------------------------------------------------------------
    // Internal per-frame state
    // -----------------------------------------------------------------------

    private static class FrameState {
        final int    frameId;
        final int    chunkCount;
        final int    totalFrameSize;
        final byte[][] chunks;
        int receivedChunks = 0;

        FrameState(int frameId, int chunkCount, int totalFrameSize) {
            this.frameId       = frameId;
            this.chunkCount    = chunkCount;
            this.totalFrameSize = totalFrameSize;
            this.chunks        = new byte[chunkCount][];
        }
    }

    // frameId → FrameState
    private final Map<Integer, FrameState> m_frames = new HashMap<>();

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------

    /**
     * Feed one SCREEN_FRAME raw payload.
     *
     * @param payload raw bytes starting at offset 0 (metadata + chunk data).
     * @return complete JPEG byte[] if this chunk completed the frame, otherwise null.
     */
    public byte[] feed(byte[] payload) {
        // --- validate minimum size ------------------------------------------
        if (payload == null || payload.length < METADATA_SIZE) {
            System.err.println("[ScreenFrameReassembler] Payload too short: "
                    + (payload == null ? "null" : payload.length));
            return null;
        }

        // --- parse metadata (Big Endian) ------------------------------------
        ByteBuffer buf = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN);
        final int frameId       = buf.getInt(0);
        final int chunkIndex    = buf.getInt(4);
        final int chunkCount    = buf.getInt(8);
        final int totalFrameSize = buf.getInt(12);

        // --- validate field values ------------------------------------------
        if (chunkCount <= 0) {
            System.err.println("[ScreenFrameReassembler] Invalid chunkCount=" + chunkCount
                    + " frameId=" + frameId);
            return null;
        }
        if (chunkIndex < 0 || chunkIndex >= chunkCount) {
            System.err.println("[ScreenFrameReassembler] chunkIndex=" + chunkIndex
                    + " out of range [0," + chunkCount + ") frameId=" + frameId);
            return null;
        }
        if (totalFrameSize <= 0) {
            System.err.println("[ScreenFrameReassembler] Invalid totalFrameSize="
                    + totalFrameSize + " frameId=" + frameId);
            return null;
        }

        final int chunkDataLen = payload.length - METADATA_SIZE;
        if (chunkDataLen < 0) {
            System.err.println("[ScreenFrameReassembler] No chunk data frameId=" + frameId);
            return null;
        }

        // --- locate or create frame state -----------------------------------
        FrameState state = m_frames.get(frameId);
        if (state == null) {
            // Evict oldest entry if we would exceed the in-flight limit
            if (m_frames.size() >= MAX_IN_FLIGHT_FRAMES) {
                int oldestKey = m_frames.keySet().iterator().next();
                m_frames.remove(oldestKey);
                System.err.println("[ScreenFrameReassembler] Evicted stale frameId=" + oldestKey);
            }
            state = new FrameState(frameId, chunkCount, totalFrameSize);
            m_frames.put(frameId, state);
        } else {
            // Validate consistency with existing state for this frameId
            if (state.chunkCount != chunkCount || state.totalFrameSize != totalFrameSize) {
                System.err.println("[ScreenFrameReassembler] Metadata mismatch for frameId="
                        + frameId + ", discarding frame.");
                m_frames.remove(frameId);
                return null;
            }
        }

        // --- store chunk (safe against duplicates) --------------------------
        if (state.chunks[chunkIndex] != null) {
            // Duplicate — ignore silently
            return null;
        }

        byte[] chunkData = new byte[chunkDataLen];
        System.arraycopy(payload, METADATA_SIZE, chunkData, 0, chunkDataLen);
        state.chunks[chunkIndex] = chunkData;
        state.receivedChunks++;

        // --- check completion -----------------------------------------------
        if (state.receivedChunks < state.chunkCount) {
            return null;  // frame not yet complete
        }

        // --- reconstruct JPEG -----------------------------------------------
        m_frames.remove(frameId);

        byte[] jpeg = new byte[totalFrameSize];
        int offset = 0;
        for (byte[] chunk : state.chunks) {
            if (chunk == null) {
                // Should not happen given receivedChunks == chunkCount
                System.err.println("[ScreenFrameReassembler] BUG: null chunk during reconstruction frameId=" + frameId);
                return null;
            }
            System.arraycopy(chunk, 0, jpeg, offset, chunk.length);
            offset += chunk.length;
        }

        // --- verify size ----------------------------------------------------
        if (offset != totalFrameSize) {
            System.err.println("[ScreenFrameReassembler] Size mismatch: reconstructed=" + offset
                    + " expected=" + totalFrameSize + " frameId=" + frameId);
            return null;
        }

        return jpeg;
    }
}
