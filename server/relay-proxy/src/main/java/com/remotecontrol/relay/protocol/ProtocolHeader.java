package com.remotecontrol.relay.protocol;

public class ProtocolHeader {

    private int magic;          // 4 bytes (uint32)
    private byte version;       // 1 byte  (uint8)
    private byte type;          // 1 byte  (uint8)
    private short flags;        // 2 bytes (uint16)
    private int payloadLength;  // 4 bytes (uint32)
    private long sessionId;     // 8 bytes (uint64)
    private int sequenceNumber; // 4 bytes (uint32)

    public static final int HEADER_LENGTH = 24;
    public static final int MAGIC = 0x52445450;
    
    public ProtocolHeader(int magic, byte version, byte type, short flags, 
                          int payloadLength, long sessionId, int sequenceNumber) {
        this.magic = magic;
        this.version = version;
        this.type = type;
        this.flags = flags;
        this.payloadLength = payloadLength;
        this.sessionId = sessionId;
        this.sequenceNumber = sequenceNumber;
    }

    public int getMagic() { return magic; }
    public void setMagic(int magic) { this.magic = magic; }

    public byte getVersion() { return version; }
    public void setVersion(byte version) { this.version = version; }

    public byte getType() { return type; }
    public void setType(byte type) { this.type = type; }

    public short getFlags() { return flags; }
    public void setFlags(short flags) { this.flags = flags; }

    public int getPayloadLength() { return payloadLength; }
    public void setPayloadLength(int payloadLength) { this.payloadLength = payloadLength; }

    public long getSessionId() { return sessionId; }
    public void setSessionId(long sessionId) { this.sessionId = sessionId; }

    public int getSequenceNumber() { return sequenceNumber; }
    public void setSequenceNumber(int sequenceNumber) { this.sequenceNumber = sequenceNumber; }

    @Override
    public String toString() {
        return "ProtocolHeader{" +
                "magic=0x" + Integer.toHexString(magic).toUpperCase() +
                ", version=" + version +
                ", type=" + type +
                ", flags=" + flags +
                ", payloadLength=" + payloadLength +
                ", sessionId=" + sessionId +
                ", seq=" + sequenceNumber +
                '}';
    }
}
