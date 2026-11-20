package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.child.*;
import jakarta.validation.Validation;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

class TelemetryValidationTest {
    private boolean valid(TelemetrySample sample) {
        try (var factory = Validation.buildDefaultValidatorFactory()) {
            var heartbeat = HeartbeatRequest.builder().os("Windows").telemetry(sample).build();
            return factory.getValidator().validate(heartbeat).isEmpty();
        }
    }
    @Test void oldClientsAndPartialWarmupAreCompatible() {
        assertTrue(valid(null));
        assertTrue(valid(TelemetrySample.builder().sequence(1L).build()));
        assertTrue(valid(TelemetrySample.builder().sequence(2L).memoryUsedBytes(0L).memoryTotalBytes(16000000000L).build()));
        assertTrue(valid(TelemetrySample.builder().sequence(3L).cpuPercent(100.0)
                .memoryUsedBytes(16000000000L).memoryTotalBytes(16000000000L)
                .networkRxBytesPerSecond(0.0).networkTxBytesPerSecond(1e9).build()));
    }
    @Test void malformedAndNonFiniteMeasurementsAreRejectedBeforeStorage() {
        for (double cpu : new double[]{-1, 100.1, Double.NaN, Double.POSITIVE_INFINITY})
            assertFalse(valid(TelemetrySample.builder().sequence(1L).cpuPercent(cpu).build()));
        for (Long sequence : new Long[]{null,0L,-1L,Long.MAX_VALUE})
            assertFalse(valid(TelemetrySample.builder().sequence(sequence).build()));
        assertFalse(valid(TelemetrySample.builder().sequence(1L).memoryUsedBytes(1001L).memoryTotalBytes(1000L).build()));
        assertFalse(valid(TelemetrySample.builder().sequence(1L).memoryUsedBytes(1L).build()));
        assertFalse(valid(TelemetrySample.builder().sequence(1L).memoryUsedBytes(0L).memoryTotalBytes(0L).build()));
        assertFalse(valid(TelemetrySample.builder().sequence(1L).memoryUsedBytes(0L).memoryTotalBytes(Long.MAX_VALUE).build()));
        assertFalse(valid(TelemetrySample.builder().sequence(1L).networkRxBytesPerSecond(1.0).build()));
        for (double rate : new double[]{-1, Double.NaN, Double.NEGATIVE_INFINITY, 1e16})
            assertFalse(valid(TelemetrySample.builder().sequence(1L).networkRxBytesPerSecond(rate).networkTxBytesPerSecond(0.0).build()));
    }
}
