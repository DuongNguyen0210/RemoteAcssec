#pragma once

#include "Streaming/StreamMetrics.h"
#include "Streaming/StreamOptimizer.h"

#include <QByteArray>
#include <QImage>
#include <QMutex>
#include <QSize>
#include <QString>
#include <QtGlobal>

#include <deque>
#include <optional>

class AdaptiveStreamController
{
public:
    struct Config
    {
        StreamOptimizer::Config optimizer;

        // Hard configurable bounds (R7)
        int minQuality = 30;
        int maxQuality = 90;
        double minScale = 0.5;
        double maxScale = 1.0;
        int minFps = 2;
        int maxFps = 15;

        // Two-tier Slow Loop cadence & event triggers (R6)
        qint64 slowLoopIntervalMs = 1000;        // clamped [500, 2000]
        qint64 minEventSlowLoopIntervalMs = 200; // prevents event thrashing
        double bandwidthChangeThreshold = 0.20;  // +/-20% BW change
        double motionChangeThreshold = 0.15;     // +/-0.15 motionScore change
        int fastMissTriggerCount = 2;            // consecutive Fast Loop misses/strains
        double cpuOverloadPercent = 85.0;        // OS CPU threshold
        double maxEncodeTimeFraction = 0.50;     // encodeMs > fraction * (1000 / activeFps)
        double rttSpikeRatio = 1.6;              // RTT >= ref * 1.6 && delta >= rttSpikeDeltaMs
        double rttSpikeDeltaMs = 30.0;
        double highRttThresholdMs = 250.0;

        // Fast Loop bounds (R6)
        int maxFastRetries = 3;                  // clamped [1, 4] retries after attempt 1
        qint64 maxFastLoopTimeMs = 35;

        // Hysteresis & anti-oscillation (R7)
        double minScoreImprovement = 0.03;
        qint64 resolutionCooldownMs = 2500;      // [2000, 3000] default 2500
        int upgradeStabilityTicks = 3;
        qint64 upgradeStabilityWindowMs = 3000;
        int qualityHysteresisStep = 5;
        double lowMotionThreshold = 0.15;
        double lowMotionScaleBias = 0.10;

        // Floor recovery & high-RTT survival (R7 + M1 Caveat #1)
        qsizetype minRecoveryWireBytes = 4096;
        double processingMarginMs = 8.0;
        double highRttFallbackMaxTxMs = 400.0;
    };

    struct FrameInput
    {
        quint64 frameId = 0;
        QImage frame;
        qint64 nowMs = 0;

        double motionScore = 0.0;
        double wQ = 0.35;
        double wR = 0.50;
        double wP = 0.15;

        double estimatedBandwidthBps = 125000.0;
        double availableBandwidthBps = 100000.0;
        double observedThroughputBps = 0.0;
        double smoothedRttMs = 0.0;
        double minRttMs = 0.0;
        bool networkCongested = false;
        qint64 pendingBytes = 0;
        double ackLatencyMs = -1.0;
        bool ackEnabled = true;

        double actualFps = -1.0; // If < 0, derived internally from sent timestamps
        std::optional<double> cpuUsagePercent;
    };

    struct FrameDecision
    {
        bool shouldSend = false;
        bool dropped = false;
        bool usedSlowLoop = false;
        bool configChanged = false;

        QByteArray jpeg;
        QSize encodedSize;
        qsizetype wireBytes = 0;
        qsizetype frameByteBudget = 0; // wire byte budget

        StreamConfig activeConfig;
        int qualityUsed = 0;
        double scaleUsed = 1.0;
        int targetFps = 0;
        int encodeRetryCount = 0;
        qint64 encodeTimeMs = 0;
        qint64 nextFrameDelayMs = 0;

        QString configChangeReason;
        StreamMetricsSnapshot metricsSnapshot;
    };

    struct StateSnapshot
    {
        bool hasActiveConfig = false;
        StreamConfig activeConfig;
        qint64 lastSlowLoopMs = -1;
        qint64 lastResolutionChangeMs = -1;
        qint64 upgradeStableSinceMs = -1;
        int upgradeStableTicks = 0;
        int consecutiveFastMisses = 0;
        qint64 droppedFrames = 0;
        qint64 slowLoopCount = 0;
        qint64 fastLoopCount = 0;
    };

    AdaptiveStreamController();
    explicit AdaptiveStreamController(const Config &config);

    void reset();
    void setConfig(const Config &config);
    Config config() const;

    void setEncodeFunction(StreamOptimizer::EncodeFunction fn);

    FrameDecision processFrame(const FrameInput &input);

    StateSnapshot state() const;
    StreamConfig activeConfig() const;

    qsizetype effectiveMaxJpegBytes() const;
    qsizetype effectiveMaxWireBytes() const;

    qsizetype computeFrameWireBudget(double availableBps,
                                     int targetFps,
                                     double smoothedRttMs = 0.0,
                                     double minRttMs = 0.0,
                                     double estimatedEncodeTimeMs = 0.0,
                                     qsizetype priorWireBytes = 0,
                                     double estimatedBps = 0.0) const;

    static qint64 computeNextFrameDelayMs(qsizetype frameBytes,
                                          qint64 encodeTimeMs,
                                          double rttMs,
                                          double availableBps,
                                          int targetFps,
                                          bool ackAlreadyWaited = true);

private:
    static Config sanitizeConfig(const Config &cfg);

    QString evaluateSlowLoopTriggerLocked(const FrameInput &input,
                                          bool &outCpuOverloaded,
                                          bool &outMotionTrigger) const;

    StreamOptimizer::OptimizerContext buildOptimizerContext(const Config &cfg,
                                                          const FrameInput &input,
                                                          double smoothedEncodeMs,
                                                          qsizetype priorWireBytes,
                                                          bool cpuOverloaded,
                                                          int activeFps) const;

    double computeActualFpsLocked(qint64 nowMs) const;

    mutable QMutex m_mutex;
    Config m_config;
    StreamOptimizer m_optimizer;

    quint64 m_resetEpoch = 0;
    bool m_hasActiveConfig = false;
    StreamConfig m_activeConfig;

    qint64 m_lastSlowLoopMs = -1;
    double m_lastSlowLoopBps = 0.0;
    double m_lastSlowLoopMotion = 0.0;
    double m_lastSlowLoopRttMs = 0.0;
    bool m_lastSlowLoopCongested = false;

    qint64 m_lastResolutionChangeMs = -1;
    qint64 m_upgradeStableSinceMs = -1;
    int m_upgradeStableTicks = 0;

    int m_consecutiveFastMisses = 0;
    double m_smoothedEncodeTimeMs = 0.0;
    bool m_hasEncodeSample = false;
    qsizetype m_lastSentWireBytes = 0;

    qint64 m_droppedFrames = 0;
    qint64 m_slowLoopCount = 0;
    qint64 m_fastLoopCount = 0;
    std::deque<qint64> m_sentTimestampsMs;
};
