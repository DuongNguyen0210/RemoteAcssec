package com.remotecontrol.relay.protocol;

public class ProtocolConstants {
    public static final int PROTOCOL_MAGIC = 0x52445450;
    public static final byte PROTOCOL_VERSION = 1;
    public static final int HEADER_SIZE = 24;
    public static final int MAX_PAYLOAD_LENGTH = 524288;

    public enum MessageType {
        REGISTER_HOST(0x01),
        REGISTER_ACK(0x02),
        CONNECT_REQUEST(0x03),
        SESSION_REQUEST(0x04),
        SESSION_ACCEPT(0x05),
        SESSION_REJECT(0x06),
        CONNECT_RESULT(0x07),

        SCREEN_FRAME(0x10),

        MOUSE_MOVE(0x20),
        MOUSE_BUTTON_DOWN(0x21),
        MOUSE_BUTTON_UP(0x22),
        MOUSE_WHEEL(0x23),

        KEY_PRESS(0x30),
        KEY_RELEASE(0x31),

        HEARTBEAT(0x40),
        PING(0x41),
        PONG(0x42),

        TELEMETRY(0x50),
        DISCONNECT(0x60),
        ERROR(0x70);

        private final byte value;

        MessageType(int value) {
            this.value = (byte) value;
        }

        public byte getValue() {
            return value;
        }

        public static MessageType fromValue(byte value) {
            for (MessageType type : values()) {
                if (type.value == value) {
                    return type;
                }
            }
            throw new IllegalArgumentException("Unknown Protocol MessageType: 0x" + String.format("%02x", value));
        }
    }
}
