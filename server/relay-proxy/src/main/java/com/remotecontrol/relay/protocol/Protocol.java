package com.remotecontrol.relay.protocol;

public class Protocol {
    private final ProtocolHeader _header;
    private final byte[] _payload;

    public Protocol(ProtocolHeader header, byte[] payload) {
        this._header = header;
        this._payload = payload;
    }

    public ProtocolHeader getHeader() {
        return _header;
    }

    public byte[] getPayload() {
        return _payload;
    }

    @Override
    public String toString() {
        return "RdtpMessage{" +
                "header=" + _header +
                ", payloadSize=" + (_payload != null ? _payload.length : 0) + " bytes" +
                '}';
    }
}
