package com.remotecontrol.api.dto.child;

/** Last accepted sample only. Reception time is assigned by the server, never the CHILD clock. */
public record TelemetrySnapshot(long sequence, Double cpuPercent, Long memoryUsedBytes, Long memoryTotalBytes,
        Double networkRxBytesPerSecond, Double networkTxBytesPerSecond, long receivedAt) {
    public static TelemetrySnapshot from(TelemetrySample sample, long now) {
        return sample == null ? null : new TelemetrySnapshot(sample.getSequence(), sample.getCpuPercent(),
                sample.getMemoryUsedBytes(), sample.getMemoryTotalBytes(), sample.getNetworkRxBytesPerSecond(),
                sample.getNetworkTxBytesPerSecond(), now);
    }
}
