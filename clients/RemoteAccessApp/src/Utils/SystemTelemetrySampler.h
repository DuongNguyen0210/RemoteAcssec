#pragma once
#include "Domain/Model/TelemetryInfo.h"
#include <QElapsedTimer>
#include <QMap>
#include <QPair>

class SystemTelemetrySampler {
public:
    struct Counters {
        qint64 monotonicMs = 0;
        std::optional<quint64> cpuIdle, cpuTotal, memoryAvailable, memoryTotal;
        bool networkAvailable = false;
        QMap<quint64, QPair<quint64, quint64>> interfaces; // Stable OS ID -> RX/TX octets.
    };
    SystemTelemetrySampler();
    void reset();
    TelemetryInfo sample();
    static TelemetryInfo calculate(const std::optional<Counters> &previous, const Counters &current);
private:
    Counters readCounters() const;
    QElapsedTimer m_clock;
    std::optional<Counters> m_previous;
};
