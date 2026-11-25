#pragma once

#include <QtGlobal>

// All times are milliseconds on the sender's monotonic clock. Byte counts include RDTP framing.
class ScreenStreamRate
{
public:
    static constexpr qsizetype MaxWireFrameBytes = 256 * 1024;
    static constexpr int MinSendIntervalMs = 100;

    explicit ScreenStreamRate(int configuredBytesPerSecond);
    void reset();
    qsizetype wireBudget() const;
    int bytesPerSecond() const { return m_bytesPerSecond; }
    qint64 lastSendMs() const { return m_lastSendMs; }
    qint64 nextSendMs() const;
    bool canProbe(qint64 nowMs) const;
    void sent(qint64 nowMs, qint64 wireBytes);

private:
    int m_bytesPerSecond;
    qint64 m_firstSendMs = -1;
    qint64 m_lastSendMs = -1;
    qint64 m_lastWireBytes = 0;
};
