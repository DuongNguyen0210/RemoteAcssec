#include "BandwidthEstimator.h"

#include <QMutexLocker>

#include <algorithm>
#include <cmath>

namespace
{
double finiteOr(double value, double fallback)
{
    return std::isfinite(value) ? value : fallback;
}
} // namespace

BandwidthEstimator::Config BandwidthEstimator::Config::fromEnvironment()
{
    Config cfg;
    cfg.initialBandwidthBps = initialBandwidthFromEnvironment(cfg.initialBandwidthBps);
    return cfg;
}

double BandwidthEstimator::initialBandwidthFromEnvironment(double fallbackBps)
{
    bool valid = false;
    const int configuredKbps = qEnvironmentVariableIntValue("REMOTE_STREAM_KBPS", &valid);
    if (!valid)
    {
        return fallbackBps;
    }
    return static_cast<double>(qBound(128, configuredKbps, 12000)) * 125.0;
}

BandwidthEstimator::Config BandwidthEstimator::sanitizeConfig(const Config &cfg)
{
    Config out = cfg;
    out.minBandwidthBps = std::max(1000.0, finiteOr(out.minBandwidthBps, 16000.0));
    out.maxBandwidthBps = std::max(out.minBandwidthBps, finiteOr(out.maxBandwidthBps, 15000000.0));
    out.initialBandwidthBps = qBound(
        out.minBandwidthBps,
        finiteOr(out.initialBandwidthBps, 125000.0),
        out.maxBandwidthBps);
    out.safetyFactor = qBound(0.1, finiteOr(out.safetyFactor, 0.8), 1.0);
    out.emaAlpha = qBound(0.01, finiteOr(out.emaAlpha, 0.25), 1.0);
    out.rttEmaAlpha = qBound(0.01, finiteOr(out.rttEmaAlpha, 0.20), 1.0);
    out.probeStepFactor = qBound(0.0, finiteOr(out.probeStepFactor, 0.08), 0.5);
    out.backoffFactor = qBound(0.1, finiteOr(out.backoffFactor, 0.70), 0.99);
    out.timeoutBackoffFactor = qBound(0.1, finiteOr(out.timeoutBackoffFactor, 0.50), 0.99);
    out.congestionRttRatio = std::max(1.05, finiteOr(out.congestionRttRatio, 1.6));
    out.congestionRttSlackMs = std::max(0.0, finiteOr(out.congestionRttSlackMs, 25.0));
    out.maxExpectedBaseRttMs = std::max(10.0, finiteOr(out.maxExpectedBaseRttMs, 250.0));
    out.maxProbeMultiplier = std::max(1.0, finiteOr(out.maxProbeMultiplier, 2.0));
    out.queueGrowthThresholdBytes = std::max<qint64>(1024, out.queueGrowthThresholdBytes);
    out.smallFrameThresholdBytes = std::max<qsizetype>(0, out.smallFrameThresholdBytes);
    out.minProbingBytes = std::max<qsizetype>(0, out.minProbingBytes);
    out.probeIntervalMs = std::max<qint64>(0, out.probeIntervalMs);
    out.probeStableAckCount = std::max(1, out.probeStableAckCount);
    out.backoffCooldownMs = std::max<qint64>(0, out.backoffCooldownMs);
    out.minRttWindowMs = std::max<qint64>(1000, out.minRttWindowMs);
    out.inFlightExpiryMs = std::max<qint64>(1000, out.inFlightExpiryMs);
    out.maxInFlightTracked = std::max(1, out.maxInFlightTracked);
    return out;
}

BandwidthEstimator::BandwidthEstimator()
    : BandwidthEstimator(Config{})
{
}

BandwidthEstimator::BandwidthEstimator(const Config &config)
    : m_config(sanitizeConfig(config)),
      m_estimatedBandwidthBps(m_config.initialBandwidthBps)
{
}

void BandwidthEstimator::reset()
{
    QMutexLocker locker(&m_mutex);
    m_estimatedBandwidthBps = m_config.initialBandwidthBps;
    m_observedThroughputBps = 0.0;
    m_smoothedRttMs = 0.0;
    m_minRttMs = 0.0;
    m_minRttFrameBytes = 0;
    m_hasAckSample = false;
    m_congested = false;
    m_cleanAckStreak = 0;
    m_lastProbeMs = -1;
    m_lastBackoffMs = -1;
    m_lastPendingBytes = 0;
    m_baselinePendingBytes = 0;
    m_lastSendTimeMs = -1;
    m_lastSentWireBytes = 0;
    m_inFlight.clear();
    m_rttWindow.clear();
}

void BandwidthEstimator::setConfig(const Config &config)
{
    {
        QMutexLocker locker(&m_mutex);
        m_config = sanitizeConfig(config);
    }
    reset();
}

void BandwidthEstimator::pruneInFlightLocked(qint64 nowMs)
{
    auto it = m_inFlight.begin();
    while (it != m_inFlight.end())
    {
        if (nowMs - it->sendTimeMs > m_config.inFlightExpiryMs)
        {
            it = m_inFlight.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void BandwidthEstimator::applyCongestionBackoffLocked(double factor, qint64 nowMs, bool bypassCooldown)
{
    m_congested = true;
    m_cleanAckStreak = 0;
    if (bypassCooldown || m_lastBackoffMs < 0 || (nowMs - m_lastBackoffMs) >= m_config.backoffCooldownMs)
    {
        const double next = m_estimatedBandwidthBps * factor;
        if (std::isfinite(next))
        {
            m_estimatedBandwidthBps = qBound(m_config.minBandwidthBps, next, m_config.maxBandwidthBps);
        }
        if (nowMs >= 0)
        {
            m_lastBackoffMs = nowMs;
        }
    }
}

void BandwidthEstimator::applyQueueCongestionLocked(qint64 pending, qint64 nowMs)
{
    applyCongestionBackoffLocked(m_config.backoffFactor, nowMs);
    m_baselinePendingBytes = pending;
    m_lastPendingBytes = pending;
}

double BandwidthEstimator::availableBandwidthBpsLocked() const
{
    return qBound(
        m_config.minBandwidthBps * m_config.safetyFactor,
        m_estimatedBandwidthBps * m_config.safetyFactor,
        m_config.maxBandwidthBps);
}

void BandwidthEstimator::onFrameSent(quint32 frameId,
                                     qsizetype wireBytes,
                                     qint64 encodeTimeMs,
                                     qint64 pendingBytesAfterWrite,
                                     qint64 nowMs)
{
    if (wireBytes <= 0)
    {
        return;
    }

    QMutexLocker locker(&m_mutex);
    pruneInFlightLocked(nowMs);

    while (m_inFlight.size() >= m_config.maxInFlightTracked && !m_inFlight.contains(frameId))
    {
        auto oldestIt = m_inFlight.begin();
        for (auto it = m_inFlight.begin(); it != m_inFlight.end(); ++it)
        {
            if (it->sendTimeMs < oldestIt->sendTimeMs)
            {
                oldestIt = it;
            }
        }
        m_inFlight.erase(oldestIt);
    }

    const qint64 clampedPending = qMax<qint64>(0, pendingBytesAfterWrite);
    m_inFlight.insert(frameId, InFlightFrame{
        frameId,
        wireBytes,
        qMax<qint64>(0, encodeTimeMs),
        clampedPending,
        nowMs,
        false,
        0
    });

    m_lastSendTimeMs = nowMs;
    m_lastSentWireBytes = wireBytes;

    const qint64 preExistingPending = qMax<qint64>(0, clampedPending - static_cast<qint64>(wireBytes));
    if (preExistingPending >= m_config.queueGrowthThresholdBytes)
    {
        applyQueueCongestionLocked(preExistingPending, nowMs);
    }
    else
    {
        m_lastPendingBytes = preExistingPending;
        if (preExistingPending < m_baselinePendingBytes)
        {
            m_baselinePendingBytes = preExistingPending;
        }
    }
}

void BandwidthEstimator::onBackpressureSample(qint64 pendingBytesNow, qint64 nowMs)
{
    QMutexLocker locker(&m_mutex);

    const qint64 pending = qMax<qint64>(0, pendingBytesNow);
    if (pending < m_baselinePendingBytes || pending == 0)
    {
        m_baselinePendingBytes = pending;
    }

    const bool highBacklog = (pending >= m_config.queueGrowthThresholdBytes);
    const bool grewPastThreshold = (pending > m_lastPendingBytes)
        && ((pending - m_baselinePendingBytes) >= m_config.queueGrowthThresholdBytes);
    const bool sameMsAsSend = (nowMs == m_lastSendTimeMs)
        && (pending <= static_cast<qint64>(m_lastSentWireBytes));

    if ((highBacklog || grewPastThreshold) && !sameMsAsSend)
    {
        applyQueueCongestionLocked(pending, nowMs);
        return;
    }

    m_lastPendingBytes = pending;
}

void BandwidthEstimator::onAckTimeout(quint32 frameId, qint64 elapsedMs, qint64 pendingBytesNow)
{
    QMutexLocker locker(&m_mutex);

    const qint64 pending = qMax<qint64>(0, pendingBytesNow);
    m_lastPendingBytes = pending;

    auto it = m_inFlight.find(frameId);
    if (it != m_inFlight.end())
    {
        const qint64 inferredNowMs = it->sendTimeMs + qMax<qint64>(0, elapsedMs);
        const bool allowBackoff = !it->timedOut
            || (elapsedMs - it->lastTimeoutElapsedMs >= m_config.backoffCooldownMs);
        it->timedOut = true;
        it->lastTimeoutElapsedMs = elapsedMs;
        if (allowBackoff)
        {
            applyCongestionBackoffLocked(m_config.timeoutBackoffFactor, inferredNowMs, true);
        }
        else
        {
            m_congested = true;
            m_cleanAckStreak = 0;
        }
    }
    else
    {
        const qint64 fallbackNowMs = (m_lastBackoffMs < 0)
            ? 0
            : (m_lastBackoffMs + m_config.backoffCooldownMs);
        applyCongestionBackoffLocked(m_config.timeoutBackoffFactor, fallbackNowMs, true);
    }
}

void BandwidthEstimator::onFrameAcknowledged(quint32 frameId, qint64 nowMs, qint64 pendingBytesNow)
{
    QMutexLocker locker(&m_mutex);

    if (!m_inFlight.contains(frameId))
    {
        return;
    }

    const InFlightFrame entry = m_inFlight.take(frameId);
    const qint64 ackDurationMs = qMax<qint64>(1, nowMs - entry.sendTimeMs);
    const qint64 pending = qMax<qint64>(0, pendingBytesNow);
    const double sampleRttMs = static_cast<double>(ackDurationMs);
    const double rawDeliveryBps = (static_cast<double>(entry.wireBytes) * 1000.0) / sampleRttMs;
    if (!std::isfinite(rawDeliveryBps) || rawDeliveryBps <= 0.0)
    {
        return;
    }

    if (!m_hasAckSample)
    {
        m_observedThroughputBps = rawDeliveryBps;
    }
    else
    {
        m_observedThroughputBps = m_config.emaAlpha * rawDeliveryBps
                                + (1.0 - m_config.emaAlpha) * m_observedThroughputBps;
    }

    bool rttInflated = false;
    double estPropRttMs = 0.0;
    if (m_hasAckSample && m_minRttMs > 0.0)
    {
        const qsizetype extraBytes = qMax<qsizetype>(0, entry.wireBytes - m_minRttFrameBytes);
        const double extraTxMs = (static_cast<double>(extraBytes) * 1000.0)
                               / qMax(m_estimatedBandwidthBps, m_config.minBandwidthBps);
        const double expectedAckMs = m_minRttMs + extraTxMs;
        const double excessMs = sampleRttMs - expectedAckMs;
        rttInflated = (sampleRttMs > expectedAckMs * m_config.congestionRttRatio)
                   && (excessMs >= m_config.congestionRttSlackMs);

        const double minRttTxMs = (static_cast<double>(m_minRttFrameBytes) * 1000.0)
                                / qMax(m_estimatedBandwidthBps, m_config.minBandwidthBps);
        estPropRttMs = qMax(0.0, m_minRttMs - minRttTxMs);
    }
    else
    {
        estPropRttMs = sampleRttMs;
    }

    if (!m_hasAckSample)
    {
        m_smoothedRttMs = sampleRttMs;
    }
    else
    {
        m_smoothedRttMs = m_config.rttEmaAlpha * sampleRttMs
                        + (1.0 - m_config.rttEmaAlpha) * m_smoothedRttMs;
    }

    m_rttWindow.push_back({nowMs, sampleRttMs, entry.wireBytes});
    while (m_rttWindow.size() > 1
           && (nowMs - m_rttWindow.front().timestampMs > m_config.minRttWindowMs
               || m_rttWindow.size() > 128))
    {
        m_rttWindow.pop_front();
    }

    m_minRttMs = m_rttWindow.front().rttMs;
    m_minRttFrameBytes = m_rttWindow.front().wireBytes;
    for (const auto &s : m_rttWindow)
    {
        if (s.rttMs < m_minRttMs)
        {
            m_minRttMs = s.rttMs;
            m_minRttFrameBytes = s.wireBytes;
        }
    }
    m_hasAckSample = true;

    const bool queueCongested = (pending >= m_config.queueGrowthThresholdBytes)
        || (entry.pendingBytesAfterWrite > static_cast<qint64>(entry.wireBytes) + m_config.queueGrowthThresholdBytes
            && pending >= entry.pendingBytesAfterWrite - static_cast<qint64>(entry.wireBytes));
    m_lastPendingBytes = pending;
    if (pending < m_baselinePendingBytes || pending == 0)
    {
        m_baselinePendingBytes = pending;
    }

    if (rttInflated || queueCongested || entry.timedOut)
    {
        if (rttInflated || queueCongested)
        {
            applyCongestionBackoffLocked(m_config.backoffFactor, nowMs);
        }
        else
        {
            m_cleanAckStreak = 0;
        }
        return;
    }

    m_congested = false;
    ++m_cleanAckStreak;

    double sampleBw = rawDeliveryBps;
    bool hasDecomposed = false;
    const double txTimeMs = sampleRttMs - estPropRttMs;
    if (entry.wireBytes > m_minRttFrameBytes
        && txTimeMs >= qMax(10.0, 0.15 * sampleRttMs))
    {
        sampleBw = (static_cast<double>(entry.wireBytes) * 1000.0) / txTimeMs;
        hasDecomposed = true;
    }
    sampleBw = qBound(m_config.minBandwidthBps, sampleBw, m_config.maxBandwidthBps);

    bool updatedUpward = false;
    const double positiveCandidate = hasDecomposed ? qMax(rawDeliveryBps, sampleBw) : rawDeliveryBps;
    if (positiveCandidate > m_estimatedBandwidthBps
        && entry.wireBytes >= 4096
        && ackDurationMs >= 2)
    {
        const double bounded = qBound(m_config.minBandwidthBps, positiveCandidate, m_config.maxBandwidthBps);
        m_estimatedBandwidthBps = qBound(
            m_config.minBandwidthBps,
            m_config.emaAlpha * bounded + (1.0 - m_config.emaAlpha) * m_estimatedBandwidthBps,
            m_config.maxBandwidthBps);
        updatedUpward = true;
    }

    bool updatedDownward = false;
    if (!updatedUpward
        && entry.wireBytes >= m_config.smallFrameThresholdBytes
        && sampleBw < m_estimatedBandwidthBps * 0.85)
    {
        const double cappedPropRttMs = qMin(estPropRttMs, m_config.maxExpectedBaseRttMs);
        const double expectedTxAtEstMs = (static_cast<double>(entry.wireBytes) * 1000.0)
                                       / qMax(m_estimatedBandwidthBps, m_config.minBandwidthBps);
        const bool txSignificant =
            (hasDecomposed && txTimeMs >= qMax(15.0, 0.25 * sampleRttMs))
            || (expectedTxAtEstMs >= 0.25 * cappedPropRttMs)
            || (ackDurationMs >= 350 && entry.wireBytes >= 65536)
            || (entry.pendingBytesAfterWrite > static_cast<qint64>(entry.wireBytes));
        if (txSignificant)
        {
            m_estimatedBandwidthBps = qBound(
                m_config.minBandwidthBps,
                m_config.emaAlpha * sampleBw + (1.0 - m_config.emaAlpha) * m_estimatedBandwidthBps,
                m_config.maxBandwidthBps);
            updatedDownward = true;
            m_cleanAckStreak = 0;
        }
    }

    const bool probeEligible = (entry.wireBytes >= m_config.minProbingBytes)
                            || (rawDeliveryBps >= availableBandwidthBpsLocked());
    const bool intervalElapsed = (m_lastProbeMs < 0)
                              || (nowMs - m_lastProbeMs >= m_config.probeIntervalMs);
    if (!updatedDownward
        && probeEligible
        && intervalElapsed
        && m_cleanAckStreak >= m_config.probeStableAckCount)
    {
        const double probeCap = qBound(
            m_config.minBandwidthBps,
            qMax(rawDeliveryBps, sampleBw) * m_config.maxProbeMultiplier,
            m_config.maxBandwidthBps);
        if (m_estimatedBandwidthBps < probeCap)
        {
            const double probed = m_estimatedBandwidthBps * (1.0 + m_config.probeStepFactor);
            m_estimatedBandwidthBps = qMin(
                probeCap,
                qBound(m_config.minBandwidthBps, probed, m_config.maxBandwidthBps));
        }
        m_lastProbeMs = nowMs;
    }
}

double BandwidthEstimator::estimatedBandwidthBps() const
{
    QMutexLocker locker(&m_mutex);
    return m_estimatedBandwidthBps;
}

double BandwidthEstimator::availableBandwidthBps() const
{
    QMutexLocker locker(&m_mutex);
    return availableBandwidthBpsLocked();
}

double BandwidthEstimator::observedThroughputBps() const
{
    QMutexLocker locker(&m_mutex);
    return m_observedThroughputBps;
}

double BandwidthEstimator::smoothedRttMs() const
{
    QMutexLocker locker(&m_mutex);
    return m_smoothedRttMs;
}

double BandwidthEstimator::minRttMs() const
{
    QMutexLocker locker(&m_mutex);
    return m_minRttMs;
}

bool BandwidthEstimator::isCongested() const
{
    QMutexLocker locker(&m_mutex);
    return m_congested;
}

int BandwidthEstimator::inFlightCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_inFlight.size();
}

BandwidthEstimator::Config BandwidthEstimator::config() const
{
    QMutexLocker locker(&m_mutex);
    return m_config;
}
