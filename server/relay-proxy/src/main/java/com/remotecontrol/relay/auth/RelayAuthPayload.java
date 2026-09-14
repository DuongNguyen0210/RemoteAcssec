package com.remotecontrol.relay.auth;

import com.remotecontrol.relay.protocol.Protocol;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.charset.CodingErrorAction;

/** v2: uint16 token length + UTF-8 token, then (CONNECT only) uint16 target length + UUID. */
public record RelayAuthPayload(String token, String targetSessionId) {
    public static RelayAuthPayload decode(Protocol message, boolean connect) {
        var header = message.getHeader();
        byte[] payload = message.getPayload();
        if (header.getFlags() != 0 || header.getSessionId() != 0 || header.getSequenceNumber() != 0
                || payload == null || header.getPayloadLength() != payload.length || payload.length > 4200) return null;
        try {
            var bytes = ByteBuffer.wrap(payload);
            String token = read(bytes, 4096);
            String target = connect ? read(bytes, 36) : null;
            if (connect && !java.util.UUID.fromString(target).toString().equals(target)) return null;
            return bytes.hasRemaining() ? null : new RelayAuthPayload(token, target);
        } catch (Exception invalid) { return null; }
    }
    private static String read(ByteBuffer bytes, int max) throws Exception {
        int length = Short.toUnsignedInt(bytes.getShort());
        if (length == 0 || length > max || length > bytes.remaining()) throw new IllegalArgumentException();
        byte[] value = new byte[length];
        bytes.get(value);
        return StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(value)).toString();
    }
}
