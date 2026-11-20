#pragma once
#include <QtGlobal>
#include <optional>

// A current machine sample, not a time-series history. Missing values are never zero-filled.
struct TelemetryInfo {
    static constexpr qint64 STALE_AFTER_MS = 10000;
    qint64 sequence = 0;
    qint64 ageMs = 0; // Server-computed age on receipt; UI adds local monotonic elapsed time.
    std::optional<double> cpuPercent;
    std::optional<quint64> memoryUsedBytes, memoryTotalBytes;
    std::optional<double> networkRxBytesPerSecond, networkTxBytesPerSecond;
};
