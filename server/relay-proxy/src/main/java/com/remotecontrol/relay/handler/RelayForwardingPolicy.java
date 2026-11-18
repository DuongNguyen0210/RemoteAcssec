package com.remotecontrol.relay.handler;

import com.remotecontrol.relay.protocol.Protocol;
import com.remotecontrol.relay.protocol.ProtocolConstants.MessageType;

/** Default-deny direction whitelist; control messages never pass through this policy. */
public final class RelayForwardingPolicy {
    private RelayForwardingPolicy() {}

    public static boolean allows(byte type, boolean fromChild) {
        if (fromChild) return type == MessageType.SCREEN_FRAME.getValue();
        return type == MessageType.MOUSE_MOVE.getValue()
                || type == MessageType.MOUSE_BUTTON_DOWN.getValue()
                || type == MessageType.MOUSE_BUTTON_UP.getValue()
                || type == MessageType.MOUSE_WHEEL.getValue()
                || type == MessageType.KEY_PRESS.getValue()
                || type == MessageType.KEY_RELEASE.getValue();
    }

    public static boolean validInput(Protocol message) {
        int expected = expectedInputLength(message.getHeader().getType());
        byte[] payload = message.getPayload();
        if (expected <= 0 || payload == null || message.getHeader().getPayloadLength() != payload.length) return false;
        if (payload.length == expected) return true;
        if (expected == 4 || payload.length != expected + 8) return false;
        var geometry = java.nio.ByteBuffer.wrap(payload, expected, 8);
        int width = geometry.getInt(), height = geometry.getInt();
        return width > 0 && height > 0 && width <= 32768 && height <= 32768;
    }

    static boolean validInputLength(byte type, int length) {
        int base = expectedInputLength(type);
        return base > 0 && (length == base || (base != 4 && length == base + 8));
    }

    static int expectedInputLength(byte type) {
        return type == MessageType.MOUSE_MOVE.getValue() ? 8
                : type == MessageType.MOUSE_BUTTON_DOWN.getValue() || type == MessageType.MOUSE_BUTTON_UP.getValue() ? 9
                : type == MessageType.MOUSE_WHEEL.getValue() ? 12
                : type == MessageType.KEY_PRESS.getValue() || type == MessageType.KEY_RELEASE.getValue() ? 4
                : -1;
    }
}
