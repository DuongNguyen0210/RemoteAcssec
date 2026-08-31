package com.remotecontrol.relay.handler.screen;

import com.remotecontrol.relay.protocol.Protocol;

/**
 * ScreenFrameHandler
 *
 * Responsibility: parse SCREEN_FRAME Protocol messages, feed ScreenFrameReassembler,
 * and log completed frames.
 *
 * One instance per RelayServerHandler (and thus per channel).
 */
public class ScreenFrameHandler {

    private final ScreenFrameReassembler m_reassembler = new ScreenFrameReassembler();

    /**
     * Handle one SCREEN_FRAME Protocol message.
     *
     * @param msg the decoded RDTP message; type must be SCREEN_FRAME.
     */
    public void handle(Protocol msg) {
        final byte[] payload = msg.getPayload();
        final int chunkCount = (payload != null && payload.length >= ScreenFrameReassembler.METADATA_SIZE)
                ? readInt(payload, 8) : 0;
        final int frameId = (payload != null && payload.length >= 4) ? readInt(payload, 0) : -1;

        byte[] jpeg = m_reassembler.feed(payload);

        if (jpeg != null) {
            System.out.println("[Relay] SCREEN_FRAME complete frameId=" + frameId
                    + " bytes=" + jpeg.length
                    + " chunks=" + chunkCount);
        }
    }

    // -----------------------------------------------------------------------
    // Big-Endian int reader (avoids importing nio just for 4 bytes)
    // -----------------------------------------------------------------------

    private static int readInt(byte[] buf, int offset) {
        return ((buf[offset]     & 0xFF) << 24)
             | ((buf[offset + 1] & 0xFF) << 16)
             | ((buf[offset + 2] & 0xFF) <<  8)
             |  (buf[offset + 3] & 0xFF);
    }
}
