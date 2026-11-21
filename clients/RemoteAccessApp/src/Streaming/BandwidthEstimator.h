#pragma once

#include <QHash>
#include <QMutex>
#include <QtGlobal>

#include <deque>

class BandwidthEstimator
{
public:
    struct Config
    {
        double initialBandwidthBps = 125000.0;      // 1000 Kbps baseline default
        double minBandwidthBps = 16000.0;           // 128 Kbps floor
        double maxBandwidthBps = 15000000.0;        // 120 Mbps ceiling
        double safetyFactor = 0.8;                  // B = estimated * safetyFactor
        double emaAlpha = 0.25;                     // Bandwidth/throughput EMA smoothing
        double rttEmaAlpha = 0.20;                  // RTT EMA smoothing
        double probeStepFactor = 0.08;              // +8% gradual upward probe per step
        double backoffFactor = 0.70;                // 30% cut on RTT/queue congestion
        double timeoutBackoffFactor = 0.50;         // 50% cut on ACK timeout/stall
        double congestionRttRatio = 1.6;            // RTT > expected * 1.6 => queuing delay
        double congestionRttSlackMs = 25.0;         // Minimum ms excess above expected RTT
        double maxExpectedBaseRttMs = 250.0;        // Cap base RTT in saturation gate
        double maxProbeMultiplier = 2.0;            // Max probe excursion above clean delivery
        qint64 queueGrowthThresholdBytes = 65536;   // 64 KiB backlog threshold
        qsizetype smallFrameThresholdBytes = 24576; // 24 KiB anti-spiral floor
        qsizetype minProbingBytes = 16384;          // 16 KiB min size for probing
        qint64 probeIntervalMs = 500;               // Min ms between upward probes
        int probeStableAckCount = 4;                // Consecutive clean ACKs before probing
        qint64 backoffCooldownMs = 300;             // Cooldown between multiplicative backoffs
        qint64 minRttWindowMs = 30000;              // Sliding window for minRttMs
        qint64 inFlightExpiryMs = 35000;            // Prune lost frames > 35s
        int maxInFlightTracked = 64;                // Bounded tracking map capacity

        static Config fromEnvironment();
    };

    BandwidthEstimator();
    explicit BandwidthEstimator(const Config &config);

    static double initialBandwidthFromEnvironment(double fallbackBps = 125000.0);

    void reset();

    void onFrameSent(quint32 frameId,
                     qsizetype wireBytes,
                     qint64 encodeTimeMs,
                     qint64 pendingBytesAfterWrite,
                     qint64 nowMs);

    void onFrameAcknowledged(quint32 frameId,
                             qint64 nowMs,
                             qint64 pendingBytesNow);

    void onAckTimeout(quint32 frameId,
                      qint64 elapsedMs,
                      qint64 pendingBytesNow);

    void onBackpressureSample(qint64 pendingBytesNow,
                              qint64 nowMs);

    double estimatedBandwidthBps() const;
    double availableBandwidthBps() const;
    double observedThroughputBps() const;
    double smoothedRttMs() const;
    double minRttMs() const;
    bool isCongested() const;
    int inFlightCount() const;
    Config config() const;

private:
    struct InFlightFrame
    {
        quint32 frameId = 0;
        qsizetype wireBytes = 0;
        qint64 encodeTimeMs = 0;
        qint64 pendingBytesAfterWrite = 0;
        qint64 sendTimeMs = 0;
        bool timedOut = false;
        qint64 lastTimeoutElapsedMs = 0;
    };

    struct RttWindowSample
    {
        qint64 timestampMs = 0;
        double rttMs = 0.0;
        qsizetype wireBytes = 0;
    };

    static Config sanitizeConfig(const Config &cfg);

    double availableBandwidthBpsLocked() const;
    void applyQueueCongestionLocked(qint64 pending, qint64 nowMs);
    void applyCongestionBackoffLocked(double factor, qint64 nowMs, bool bypassCooldown = false);
    void pruneInFlightLocked(qint64 nowMs);

    mutable QMutex m_mutex;
    Config m_config;
    double m_estimatedBandwidthBps = 125000.0;
    double m_observedThroughputBps = 0.0;
    double m_smoothedRttMs = 0.0;
    double m_minRttMs = 0.0;
    qsizetype m_minRttFrameBytes = 0;
    bool m_hasAckSample = false;
    bool m_congested = false;
    int m_cleanAckStreak = 0;
    qint64 m_lastProbeMs = -1;
    qint64 m_lastBackoffMs = -1;
    qint64 m_lastPendingBytes = 0;
    qint64 m_baselinePendingBytes = 0;
    qint64 m_lastSendTimeMs = -1;
    qsizetype m_lastSentWireBytes = 0;
    QHash<quint32, InFlightFrame> m_inFlight;
    std::deque<RttWindowSample> m_rttWindow;
};
