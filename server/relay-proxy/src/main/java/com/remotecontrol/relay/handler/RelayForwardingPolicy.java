package com.remotecontrol.relay.handler;

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
}
