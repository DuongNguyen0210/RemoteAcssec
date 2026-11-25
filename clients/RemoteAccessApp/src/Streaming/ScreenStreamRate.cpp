#include "ScreenStreamRate.h"

ScreenStreamRate::ScreenStreamRate(int configuredBytesPerSecond)
    : m_bytesPerSecond(qMax(1, configuredBytesPerSecond))
{
}

void ScreenStreamRate::reset()
{
    *this = ScreenStreamRate(m_bytesPerSecond);
}

qsizetype ScreenStreamRate::wireBudget() const
{
    // Reserve one second's worth of bytes for a single sharp frame.
    return qMin(MaxWireFrameBytes, qsizetype(m_bytesPerSecond));
}

qint64 ScreenStreamRate::nextSendMs() const
{
    if (m_lastSendMs < 0) return 0;
    const auto byteDelayMs = (1000 * m_lastWireBytes + m_bytesPerSecond - 1) / m_bytesPerSecond;
    return m_lastSendMs + qMax(qint64(MinSendIntervalMs), byteDelayMs);
}

bool ScreenStreamRate::canProbe(qint64 nowMs) const
{
    // A slow ACK will already hold the frame window closed.
    return m_firstSendMs >= 0 && nowMs - m_firstSendMs >= 2000;
}

void ScreenStreamRate::sent(qint64 nowMs, qint64 wireBytes)
{
    if (m_firstSendMs < 0) m_firstSendMs = nowMs;
    m_lastSendMs = nowMs;
    m_lastWireBytes = wireBytes;
}
