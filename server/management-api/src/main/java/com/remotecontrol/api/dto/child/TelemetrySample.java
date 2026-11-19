package com.remotecontrol.api.dto.child;

import com.fasterxml.jackson.annotation.JsonIgnore;
import jakarta.validation.constraints.*;
import lombok.*;

@Data @Builder @NoArgsConstructor @AllArgsConstructor
public class TelemetrySample {
    @NotNull @Min(1) @Max(9007199254740991L)
    private Long sequence;
    private Double cpuPercent;
    private Long memoryUsedBytes;
    private Long memoryTotalBytes;
    private Double networkRxBytesPerSecond;
    private Double networkTxBytesPerSecond;

    private static boolean bounded(Double value, double maximum) {
        return value == null || (Double.isFinite(value) && value >= 0 && value <= maximum);
    }
    @AssertTrue(message = "Telemetry values are out of range or incomplete")
    @JsonIgnore
    public boolean isValidTelemetry() {
        boolean memory = memoryTotalBytes == null && memoryUsedBytes == null
                || memoryTotalBytes != null && memoryUsedBytes != null
                && memoryTotalBytes > 0 && memoryTotalBytes <= 9007199254740991L
                && memoryUsedBytes >= 0 && memoryUsedBytes <= memoryTotalBytes;
        return memory && bounded(cpuPercent, 100)
                && (networkRxBytesPerSecond == null) == (networkTxBytesPerSecond == null)
                && bounded(networkRxBytesPerSecond, 1e15) && bounded(networkTxBytesPerSecond, 1e15);
    }
}
