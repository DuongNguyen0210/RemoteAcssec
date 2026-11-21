#include "Streaming/AdaptiveStreamController.h"

#include <QMutexLocker>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
double finiteOr(double value, double fallback)
{
    return std::isfinite(value) ? value : fallback;
}
} // namespace

AdaptiveStreamController::Config AdaptiveStreamController::sanitizeConfig(const Config &cfg)
{
    Config out = cfg;

    out.minQuality = std::clamp(cfg.minQuality, ScreenEncoder::MIN_QUALITY, ScreenEncoder::MAX_QUALITY);
    out.maxQuality = std::clamp(cfg.maxQuality, out.minQuality, ScreenEncoder::MAX_QUALITY);
    out.minScale = std::clamp(finiteOr(cfg.minScale, 0.5), 0.1, 1.0);
    out.maxScale = std::clamp(finiteOr(cfg.maxScale, 1.0), out.minScale, 1.0);
    out.minFps = std::clamp(cfg.minFps, 1, 120);
    out.maxFps = std::clamp(cfg.maxFps, out.minFps, 120);

    out.slowLoopIntervalMs = std::clamp<qint64>(cfg.slowLoopIntervalMs, 500, 2000);
    out.minEventSlowLoopIntervalMs = std::clamp<qint64>(cfg.minEventSlowLoopIntervalMs, 0, out.slowLoopIntervalMs);
    out.bandwidthChangeThreshold = std::clamp(finiteOr(cfg.bandwidthChangeThreshold, 0.20), 0.01, 0.95);
    out.motionChangeThreshold = std::clamp(finiteOr(cfg.motionChangeThreshold, 0.15), 0.01, 1.0);
    out.fastMissTriggerCount = std::clamp(cfg.fastMissTriggerCount, 1, 10);
    out.cpuOverloadPercent = std::clamp(finiteOr(cfg.cpuOverloadPercent, 85.0), 10.0, 100.0);
    out.maxEncodeTimeFraction = std::clamp(finiteOr(cfg.maxEncodeTimeFraction, 0.50), 0.05, 1.0);
    out.rttSpikeRatio = std::max(1.05, finiteOr(cfg.rttSpikeRatio, 1.6));
    out.rttSpikeDeltaMs = std::max(0.0, finiteOr(cfg.rttSpikeDeltaMs, 30.0));
    out.highRttThresholdMs = std::max(10.0, finiteOr(cfg.highRttThresholdMs, 250.0));

    out.maxFastRetries = std::clamp(cfg.maxFastRetries, 1, 4);
    out.maxFastLoopTimeMs = std::max<qint64>(1, cfg.maxFastLoopTimeMs);

    out.minScoreImprovement = std::clamp(finiteOr(cfg.minScoreImprovement, 0.03), 0.0, 1.0);
    out.resolutionCooldownMs = std::max<qint64>(0, cfg.resolutionCooldownMs);
    out.upgradeStabilityTicks = std::max(1, cfg.upgradeStabilityTicks);
    out.upgradeStabilityWindowMs = std::max<qint64>(0, cfg.upgradeStabilityWindowMs);
    out.qualityHysteresisStep = std::clamp(cfg.qualityHysteresisStep, 0, 50);
    out.lowMotionThreshold = std::clamp(finiteOr(cfg.lowMotionThreshold, 0.15), 0.0, 1.0);
    out.lowMotionScaleBias = std::clamp(finiteOr(cfg.lowMotionScaleBias, 0.10), 0.0, 1.0);

    out.minRecoveryWireBytes = std::max<qsizetype>(0, cfg.minRecoveryWireBytes);
    out.processingMarginMs = std::max(0.0, finiteOr(cfg.processingMarginMs, 8.0));
    out.highRttFallbackMaxTxMs = std::max(10.0, finiteOr(cfg.highRttFallbackMaxTxMs, 400.0));

    out.optimizer.minQuality = out.minQuality;
    out.optimizer.maxQuality = out.maxQuality;
    out.optimizer.processingMarginMs = out.processingMarginMs;
    out.optimizer.highRttFallbackMaxTxMs = out.highRttFallbackMaxTxMs;

    return out;
}

AdaptiveStreamController::AdaptiveStreamController()
    : AdaptiveStreamController(Config{})
{
}

AdaptiveStreamController::AdaptiveStreamController(const Config &config)
    : m_config(sanitizeConfig(config)),
      m_optimizer(m_config.optimizer)
{
}

void AdaptiveStreamController::reset()
{
    QMutexLocker locker(&m_mutex);
    ++m_resetEpoch;
    m_hasActiveConfig = false;
    m_activeConfig = StreamConfig{};
    m_lastSlowLoopMs = -1;
    m_lastSlowLoopBps = 0.0;
    m_lastSlowLoopMotion = 0.0;
    m_lastSlowLoopRttMs = 0.0;
    m_lastSlowLoopCongested = false;
    m_lastResolutionChangeMs = -1;
    m_upgradeStableSinceMs = -1;
    m_upgradeStableTicks = 0;
    m_consecutiveFastMisses = 0;
    m_smoothedEncodeTimeMs = 0.0;
    m_hasEncodeSample = false;
    m_lastSentWireBytes = 0;
    m_droppedFrames = 0;
    m_slowLoopCount = 0;
    m_fastLoopCount = 0;
    m_sentTimestampsMs.clear();
    m_optimizer.clearCaches();
}

void AdaptiveStreamController::setConfig(const Config &config)
{
    QMutexLocker locker(&m_mutex);
    ++m_resetEpoch;
    m_config = sanitizeConfig(config);
    m_optimizer.setConfig(m_config.optimizer);
}

AdaptiveStreamController::Config AdaptiveStreamController::config() const
{
    QMutexLocker locker(&m_mutex);
    return m_config;
}

void AdaptiveStreamController::setEncodeFunction(StreamOptimizer::EncodeFunction fn)
{
    QMutexLocker locker(&m_mutex);
    ++m_resetEpoch;
    m_optimizer.setEncodeFunction(std::move(fn));
}

qsizetype AdaptiveStreamController::effectiveMaxJpegBytes() const
{
    QMutexLocker locker(&m_mutex);
    return m_optimizer.effectiveMaxPayload();
}

qsizetype AdaptiveStreamController::effectiveMaxWireBytes() const
{
    QMutexLocker locker(&m_mutex);
    return m_optimizer.effectiveMaxWireBytes();
}

qsizetype AdaptiveStreamController::computeFrameWireBudget(double availableBps,
                                                           int targetFps,
                                                           double smoothedRttMs,
                                                           double minRttMs,
                                                           double estimatedEncodeTimeMs,
                                                           qsizetype priorWireBytes,
                                                           double estimatedBps) const
{
    QMutexLocker locker(&m_mutex);
    StreamOptimizer::OptimizerContext ctx;
    ctx.availableBandwidthBps = availableBps;
    ctx.estimatedBandwidthBps = (estimatedBps > 0.0) ? estimatedBps : availableBps;
    ctx.smoothedRttMs = smoothedRttMs;
    ctx.minRttMs = minRttMs;
    ctx.estimatedEncodeTimeMs = estimatedEncodeTimeMs;
    ctx.priorWireBytes = priorWireBytes;
    ctx.minFps = m_config.minFps;
    ctx.maxFps = m_config.maxFps;
    ctx.minRecoveryWireBytes = m_config.minRecoveryWireBytes;
    ctx.allowHighRttFallback = true;
    return m_optimizer.computeMaxWireBytes(targetFps, ctx);
}

qint64 AdaptiveStreamController::computeNextFrameDelayMs(qsizetype frameBytes,
                                                         qint64 encodeTimeMs,
                                                         double rttMs,
                                                         double availableBps,
                                                         int targetFps,
                                                         bool ackAlreadyWaited)
{
    const int safeFps = std::max(1, targetFps);
    const double targetIntervalMs = 1000.0 / static_cast<double>(safeFps);
    const double safeBps = (std::isfinite(availableBps) && availableBps > 0.0) ? availableBps : 0.0;
    const double txTimeMs = (frameBytes > 0 && safeBps > 0.0)
        ? (static_cast<double>(frameBytes) * 1000.0) / safeBps
        : 0.0;
    const double pacedIntervalMs = std::max(targetIntervalMs, txTimeMs);
    const double encMs = static_cast<double>(std::max<qint64>(0, encodeTimeMs));
    const double safeRttMs = (std::isfinite(rttMs) && rttMs > 0.0) ? rttMs : 0.0;

    if (ackAlreadyWaited)
    {
        // smoothedRttMs already includes transmission time + propagation RTT + admin processing.
        // Taking max(txTimeMs, safeRttMs) avoids double-counting serialization delay and RTT.
        const double inFlightMs = std::max(txTimeMs, safeRttMs);
        return std::max<qint64>(0, std::llround(pacedIntervalMs - encMs - inFlightMs));
    }

    return std::max<qint64>(0, std::llround(pacedIntervalMs - encMs));
}

QString AdaptiveStreamController::evaluateSlowLoopTriggerLocked(const FrameInput &input,
                                                                bool &outCpuOverloaded,
                                                                bool &outMotionTrigger) const
{
    outCpuOverloaded = false;
    outMotionTrigger = false;

    if (!m_hasActiveConfig)
    {
        return QStringLiteral("init");
    }

    const qint64 elapsedSinceSlow = (m_lastSlowLoopMs >= 0) ? (input.nowMs - m_lastSlowLoopMs) : 0;
    const bool allowEvent = (m_lastSlowLoopMs < 0) || (elapsedSinceSlow >= m_config.minEventSlowLoopIntervalMs);

    const double activeIntervalMs = 1000.0 / static_cast<double>(std::max(1, m_activeConfig.fps));
    const bool cpuHigh = input.cpuUsagePercent.has_value()
        && std::isfinite(*input.cpuUsagePercent)
        && (*input.cpuUsagePercent >= m_config.cpuOverloadPercent);
    const bool encodeSlow = m_hasEncodeSample
        && (m_smoothedEncodeTimeMs > activeIntervalMs * m_config.maxEncodeTimeFraction);
    outCpuOverloaded = cpuHigh || encodeSlow;

    if (allowEvent && outCpuOverloaded)
    {
        return QStringLiteral("cpu_overload");
    }

    const double effectiveRtt = (std::isfinite(input.smoothedRttMs) && input.smoothedRttMs > 0.0)
        ? input.smoothedRttMs
        : ((std::isfinite(input.minRttMs) && input.minRttMs > 0.0) ? input.minRttMs : 0.0);
    const bool crossedHighRtt = (effectiveRtt >= m_config.highRttThresholdMs)
        && (m_lastSlowLoopRttMs < m_config.highRttThresholdMs);
    const bool rttSpiked = (m_lastSlowLoopRttMs > 0.0)
        && (effectiveRtt >= m_lastSlowLoopRttMs * m_config.rttSpikeRatio)
        && ((effectiveRtt - m_lastSlowLoopRttMs) >= m_config.rttSpikeDeltaMs);
    if (allowEvent && (crossedHighRtt || rttSpiked))
    {
        return QStringLiteral("rtt_spike");
    }

    const double availBps = std::max(1.0, finiteOr(input.availableBandwidthBps, 1.0));
    const bool bwDropped = (m_lastSlowLoopBps > 0.0)
        && (availBps <= m_lastSlowLoopBps * (1.0 - m_config.bandwidthChangeThreshold));
    const bool newlyCongested = input.networkCongested && !m_lastSlowLoopCongested;
    if (allowEvent && (bwDropped || newlyCongested))
    {
        return QStringLiteral("bw_drop");
    }

    if (allowEvent && m_consecutiveFastMisses >= m_config.fastMissTriggerCount)
    {
        return QStringLiteral("fast_miss");
    }

    const double motion = std::clamp(finiteOr(input.motionScore, 0.0), 0.0, 1.0);
    if (allowEvent && std::abs(motion - m_lastSlowLoopMotion) >= m_config.motionChangeThreshold)
    {
        outMotionTrigger = true;
        return (motion > m_lastSlowLoopMotion)
            ? QStringLiteral("motion_up")
            : QStringLiteral("motion_down");
    }

    const bool bwRose = (m_lastSlowLoopBps > 0.0)
        && (availBps >= m_lastSlowLoopBps * (1.0 + m_config.bandwidthChangeThreshold));
    if (allowEvent && bwRose)
    {
        return QStringLiteral("bw_rise");
    }

    if (m_lastSlowLoopMs < 0 || elapsedSinceSlow >= m_config.slowLoopIntervalMs)
    {
        return QStringLiteral("periodic");
    }

    return QString();
}

StreamOptimizer::OptimizerContext AdaptiveStreamController::buildOptimizerContext(
    const Config &cfg,
    const FrameInput &input,
    double smoothedEncodeMs,
    qsizetype priorWireBytes,
    bool cpuOverloaded,
    int activeFps) const
{
    StreamOptimizer::OptimizerContext ctx;
    ctx.availableBandwidthBps = std::max(1.0, finiteOr(input.availableBandwidthBps, 1.0));
    ctx.estimatedBandwidthBps = std::max(ctx.availableBandwidthBps,
                                         finiteOr(input.estimatedBandwidthBps, ctx.availableBandwidthBps));
    ctx.smoothedRttMs = std::max(0.0, finiteOr(input.smoothedRttMs, 0.0));
    ctx.minRttMs = std::max(0.0, finiteOr(input.minRttMs, 0.0));
    ctx.estimatedEncodeTimeMs = std::max(0.0, smoothedEncodeMs);
    ctx.priorWireBytes = std::max<qsizetype>(0, priorWireBytes);
    ctx.wQ = input.wQ;
    ctx.wR = input.wR;
    ctx.wP = input.wP;

    const double motion = std::clamp(finiteOr(input.motionScore, 0.0), 0.0, 1.0);
    ctx.lowMotionScaleBias = (motion <= cfg.lowMotionThreshold) ? cfg.lowMotionScaleBias : 0.0;

    ctx.minQuality = cfg.minQuality;
    ctx.maxQuality = cfg.maxQuality;
    ctx.minScale = cfg.minScale;
    ctx.maxScale = cfg.maxScale;
    ctx.minFps = cfg.minFps;
    ctx.maxFps = cfg.maxFps;
    ctx.minRecoveryWireBytes = cfg.minRecoveryWireBytes;
    ctx.allowHighRttFallback = true;

    if (cpuOverloaded)
    {
        int lowerFpsCap = cfg.minFps;
        for (int f : cfg.optimizer.fpsLevels)
        {
            if (f < activeFps && f >= cfg.minFps)
            {
                lowerFpsCap = std::max(lowerFpsCap, f);
            }
        }
        ctx.maxFps = std::clamp(lowerFpsCap, cfg.minFps, cfg.maxFps);
    }

    return ctx;
}

double AdaptiveStreamController::computeActualFpsLocked(qint64 nowMs) const
{
    if (m_sentTimestampsMs.size() < 2)
    {
        return m_sentTimestampsMs.empty() ? 0.0 : 1.0;
    }
    const qint64 spanMs = std::max<qint64>(1, nowMs - m_sentTimestampsMs.front());
    const double seconds = static_cast<double>(spanMs) / 1000.0;
    return static_cast<double>(m_sentTimestampsMs.size() - 1) / std::max(0.1, seconds);
}

AdaptiveStreamController::FrameDecision AdaptiveStreamController::processFrame(const FrameInput &input)
{
    FrameDecision decision;

    Config cfgSnapshot;
    StreamOptimizer localOptimizer;
    quint64 entryEpoch = 0;
    bool hasActive = false;
    StreamConfig priorActive;
    qint64 lastResChangeMs = -1;
    qint64 stableSinceMs = -1;
    int stableTicks = 0;
    int consecutiveMisses = 0;
    double smoothedEncodeMs = 0.0;
    qsizetype priorWireBytes = 0;
    QString slowTrigger;
    bool cpuOverloaded = false;
    bool motionTrigger = false;

    {
        QMutexLocker locker(&m_mutex);
        entryEpoch = m_resetEpoch;
        cfgSnapshot = m_config;
        localOptimizer = m_optimizer;
        hasActive = m_hasActiveConfig;
        priorActive = m_activeConfig;
        lastResChangeMs = m_lastResolutionChangeMs;
        stableSinceMs = m_upgradeStableSinceMs;
        stableTicks = m_upgradeStableTicks;
        consecutiveMisses = m_consecutiveFastMisses;
        smoothedEncodeMs = m_smoothedEncodeTimeMs;
        priorWireBytes = m_lastSentWireBytes;

        slowTrigger = evaluateSlowLoopTriggerLocked(input, cpuOverloaded, motionTrigger);
    }

    if (input.frame.isNull() || input.frame.width() <= 0 || input.frame.height() <= 0)
    {
        QMutexLocker locker(&m_mutex);
        if (entryEpoch == m_resetEpoch)
        {
            ++m_droppedFrames;
            decision.dropped = true;
            decision.shouldSend = false;
            decision.configChangeReason = QStringLiteral("frame_drop");
            decision.nextFrameDelayMs = 1000 / std::max(1, cfgSnapshot.minFps);
            decision.activeConfig = m_activeConfig;
            decision.metricsSnapshot = StreamMetricsSnapshot{
                input.estimatedBandwidthBps,
                input.observedThroughputBps,
                0,
                0,
                std::clamp(finiteOr(input.motionScore, 0.0), 0.0, 1.0),
                0,
                QSize(),
                m_activeConfig.fps,
                input.actualFps >= 0.0 ? input.actualFps : computeActualFpsLocked(input.nowMs),
                0,
                0,
                input.ackLatencyMs,
                input.pendingBytes,
                m_droppedFrames,
                decision.configChangeReason
            };
        }
        return decision;
    }

    const qsizetype overhead = localOptimizer.config().headerOverheadBytes;
    const qsizetype maxAllowedWire = localOptimizer.effectiveMaxWireBytes();
    const double rttForPacing = (std::isfinite(input.smoothedRttMs) && input.smoothedRttMs > 0.0)
        ? input.smoothedRttMs
        : std::max(0.0, finiteOr(input.minRttMs, 0.0));

    StreamConfig nextActive = priorActive;
    bool nextHasActive = hasActive;
    qint64 nextResChangeMs = lastResChangeMs;
    qint64 nextStableSinceMs = stableSinceMs;
    int nextStableTicks = stableTicks;
    int nextConsecutiveMisses = consecutiveMisses;
    EncodeAttemptResult chosenEncode;
    qsizetype chosenWireBudget = 0;
    bool usedSlowLoop = !slowTrigger.isEmpty();
    bool configChanged = false;
    QString changeReason;
    bool frameDropped = false;

    if (usedSlowLoop)
    {
        StreamOptimizer::OptimizerContext optCtx = buildOptimizerContext(
            cfgSnapshot,
            input,
            smoothedEncodeMs,
            priorWireBytes,
            cpuOverloaded,
            hasActive ? priorActive.fps : cfgSnapshot.maxFps);

        if (hasActive)
        {
            optCtx.warmStartQuality = priorActive.quality;
            optCtx.warmStartScale = priorActive.scale;
            optCtx.warmStartFps = priorActive.fps;
        }

        const StreamOptimizer::OptimizeResult optRes =
            localOptimizer.optimizeFull(input.frameId, input.frame, optCtx);

        if (!hasActive)
        {
            if (optRes.feasible && optRes.encoded.valid)
            {
                nextActive = optRes.bestConfig;
                nextHasActive = true;
                chosenEncode = optRes.encoded;
                chosenWireBudget = optRes.frameByteBudget;
                configChanged = true;
                changeReason = QStringLiteral("init");
            }
            else
            {
                // Explicit safe fallback at (minScale, minQuality, minFps)
                const qsizetype minWireBudget = localOptimizer.computeMaxWireBytes(cfgSnapshot.minFps, optCtx);
                const qsizetype minJpegBudget = (minWireBudget > overhead) ? (minWireBudget - overhead) : 0;
                const EncodeAttemptResult fb = localOptimizer.encodeExact(
                    input.frameId, input.frame, cfgSnapshot.minScale, cfgSnapshot.minQuality, minJpegBudget);
                if (fb.valid && fb.wireBytes <= minWireBudget && fb.wireBytes <= maxAllowedWire)
                {
                    nextActive = StreamConfig{
                        cfgSnapshot.minQuality,
                        cfgSnapshot.minScale,
                        cfgSnapshot.minFps,
                        localOptimizer.computeObjectiveScore(
                            cfgSnapshot.minQuality, cfgSnapshot.minScale, cfgSnapshot.minFps, optCtx),
                        fb.wireBytes
                    };
                    nextHasActive = true;
                    chosenEncode = fb;
                    chosenWireBudget = minWireBudget;
                    configChanged = true;
                    changeReason = QStringLiteral("safe_fallback");
                }
                else
                {
                    frameDropped = true;
                    changeReason = QStringLiteral("frame_drop");
                }
            }
        }
        else
        {
            // Re-evaluate activeConfig feasibility & current-weight objective score
            StreamOptimizer::OptimizerContext pinnedActiveCtx = optCtx;
            pinnedActiveCtx.minScale = priorActive.scale;
            pinnedActiveCtx.maxScale = priorActive.scale;
            StreamOptimizer::OptimizeResult activeScaleOpt =
                localOptimizer.optimizeFull(input.frameId, input.frame, pinnedActiveCtx);
            bool activeScaleFeasible = activeScaleOpt.feasible && activeScaleOpt.encoded.valid;
            if (!activeScaleFeasible && cpuOverloaded)
            {
                StreamOptimizer::OptimizerContext fallbackActiveCtx = pinnedActiveCtx;
                fallbackActiveCtx.maxFps = cfgSnapshot.maxFps;
                const StreamOptimizer::OptimizeResult fallbackOpt =
                    localOptimizer.optimizeFull(input.frameId, input.frame, fallbackActiveCtx);
                activeScaleFeasible = fallbackOpt.feasible && fallbackOpt.encoded.valid;
            }
            const bool isEmergency = !activeScaleFeasible;

            const double activeCurrentScore = localOptimizer.computeObjectiveScore(
                priorActive.quality, priorActive.scale, priorActive.fps, optCtx);

            if (!optRes.feasible || !optRes.encoded.valid)
            {
                // Safe fallback check at (minScale, minQuality, minFps)
                const qsizetype minWireBudget = localOptimizer.computeMaxWireBytes(cfgSnapshot.minFps, optCtx);
                const qsizetype minJpegBudget = (minWireBudget > overhead) ? (minWireBudget - overhead) : 0;
                const EncodeAttemptResult fb = localOptimizer.encodeExact(
                    input.frameId, input.frame, cfgSnapshot.minScale, cfgSnapshot.minQuality, minJpegBudget);
                if (fb.valid && fb.wireBytes <= minWireBudget && fb.wireBytes <= maxAllowedWire)
                {
                    const bool scaleChanged = std::abs(cfgSnapshot.minScale - priorActive.scale) > 1e-6;
                    if (scaleChanged)
                    {
                        nextResChangeMs = input.nowMs;
                    }
                    nextActive = StreamConfig{
                        cfgSnapshot.minQuality,
                        cfgSnapshot.minScale,
                        cfgSnapshot.minFps,
                        localOptimizer.computeObjectiveScore(
                            cfgSnapshot.minQuality, cfgSnapshot.minScale, cfgSnapshot.minFps, optCtx),
                        fb.wireBytes
                    };
                    chosenEncode = fb;
                    chosenWireBudget = minWireBudget;
                    configChanged = (nextActive.quality != priorActive.quality)
                        || (nextActive.fps != priorActive.fps)
                        || scaleChanged;
                    changeReason = configChanged ? QStringLiteral("safe_fallback") : QString();
                    nextStableTicks = 0;
                    nextStableSinceMs = -1;
                    nextConsecutiveMisses = 0;
                }
                else
                {
                    frameDropped = true;
                    changeReason = QStringLiteral("frame_drop");
                }
            }
            else
            {
                StreamConfig cand = optRes.bestConfig;
                EncodeAttemptResult candEncode = optRes.encoded;
                qsizetype candBudget = optRes.frameByteBudget;

                const bool insideResCooldown = (nextResChangeMs >= 0)
                    && ((input.nowMs - nextResChangeMs) < cfgSnapshot.resolutionCooldownMs);

                const bool wantsScaleChange = std::abs(cand.scale - priorActive.scale) > 1e-6;
                if (wantsScaleChange && insideResCooldown && !isEmergency && activeScaleFeasible)
                {
                    cand = activeScaleOpt.bestConfig;
                    candEncode = activeScaleOpt.encoded;
                    candBudget = activeScaleOpt.frameByteBudget;
                }

                const bool candScaleDiff = std::abs(cand.scale - priorActive.scale) > 1e-6;
                const bool isDowngrade = (cand.scale < priorActive.scale - 1e-6)
                    || (cand.fps < priorActive.fps)
                    || (cand.quality < priorActive.quality - cfgSnapshot.qualityHysteresisStep)
                    || isEmergency;
                const bool isPureMotionTradeoff = motionTrigger
                    && (cand.scale < priorActive.scale - 1e-6)
                    && (cand.fps > priorActive.fps);
                const bool isUpgrade = !isDowngrade && !isPureMotionTradeoff
                    && ((cand.scale > priorActive.scale + 1e-6)
                        || (cand.fps > priorActive.fps)
                        || (cand.quality > priorActive.quality + cfgSnapshot.qualityHysteresisStep));

                bool acceptCandidate = false;
                if (isEmergency)
                {
                    acceptCandidate = true;
                    nextStableTicks = 0;
                    nextStableSinceMs = -1;
                }
                else if (isDowngrade || isPureMotionTradeoff)
                {
                    nextStableTicks = 0;
                    nextStableSinceMs = -1;
                    const bool meaningfulChange = candScaleDiff
                        || (cand.fps != priorActive.fps)
                        || (std::abs(cand.quality - priorActive.quality) >= cfgSnapshot.qualityHysteresisStep);
                    if (meaningfulChange)
                    {
                        if (isPureMotionTradeoff)
                        {
                            acceptCandidate = (cand.objectiveScore - activeCurrentScore >= cfgSnapshot.minScoreImprovement);
                        }
                        else
                        {
                            acceptCandidate = true;
                        }
                    }
                }
                else if (isUpgrade)
                {
                    if (input.networkCongested || consecutiveMisses > 0)
                    {
                        nextStableTicks = 0;
                        nextStableSinceMs = -1;
                    }
                    else
                    {
                        if (nextStableSinceMs < 0)
                        {
                            nextStableSinceMs = input.nowMs;
                        }
                        ++nextStableTicks;

                        const bool stabilityMet = (nextStableTicks >= cfgSnapshot.upgradeStabilityTicks)
                            && ((input.nowMs - nextStableSinceMs) >= cfgSnapshot.upgradeStabilityWindowMs);
                        const bool scoreMet =
                            (cand.objectiveScore - activeCurrentScore) >= cfgSnapshot.minScoreImprovement;
                        const bool resCooldownOk = !(cand.scale > priorActive.scale + 1e-6 && insideResCooldown);

                        if (stabilityMet && scoreMet && resCooldownOk)
                        {
                            acceptCandidate = true;
                        }
                        else if (!resCooldownOk && stabilityMet && activeScaleFeasible)
                        {
                            // Upgrade q/p at current scale if scale cooldown hasn't expired
                            const StreamConfig &sameScaleCand = activeScaleOpt.bestConfig;
                            if ((sameScaleCand.fps > priorActive.fps ||
                                 sameScaleCand.quality > priorActive.quality + cfgSnapshot.qualityHysteresisStep) &&
                                (sameScaleCand.objectiveScore - activeCurrentScore >= cfgSnapshot.minScoreImprovement))
                            {
                                cand = sameScaleCand;
                                candEncode = activeScaleOpt.encoded;
                                candBudget = activeScaleOpt.frameByteBudget;
                                acceptCandidate = true;
                            }
                        }
                    }
                }

                if (acceptCandidate)
                {
                    const bool scaleChangedNow = std::abs(cand.scale - priorActive.scale) > 1e-6;
                    if (scaleChangedNow)
                    {
                        nextResChangeMs = input.nowMs;
                    }
                    nextActive = cand;
                    chosenEncode = candEncode;
                    chosenWireBudget = candBudget;
                    configChanged = (cand.quality != priorActive.quality)
                        || (cand.fps != priorActive.fps)
                        || scaleChangedNow;
                    if (isEmergency && scaleChangedNow)
                    {
                        changeReason = QStringLiteral("emergency_drop");
                    }
                    else if (configChanged)
                    {
                        changeReason = slowTrigger;
                    }
                    if (isUpgrade)
                    {
                        nextStableTicks = 0;
                        nextStableSinceMs = -1;
                    }
                    nextConsecutiveMisses = 0;
                }
                else
                {
                    // Hysteresis keeps priorActive: encode/send using priorActive's config & budget
                    const qsizetype activeWireBudget = localOptimizer.computeMaxWireBytes(priorActive.fps, optCtx);
                    const qsizetype activeJpegBudget =
                        (activeWireBudget > overhead) ? (activeWireBudget - overhead) : 0;

                    StreamOptimizer::FastContext fastCtx;
                    fastCtx.maxWireBytes = activeWireBudget;
                    fastCtx.maxJpegBytes = activeJpegBudget;
                    fastCtx.minQuality = cfgSnapshot.minQuality;
                    fastCtx.maxQuality = cfgSnapshot.maxQuality;
                    fastCtx.maxRetries = cfgSnapshot.maxFastRetries;
                    fastCtx.maxTimeMs = cfgSnapshot.maxFastLoopTimeMs;

                    EncodeAttemptResult heldEncode =
                        localOptimizer.encodeFast(input.frameId, input.frame, priorActive, fastCtx);
                    if (heldEncode.valid && heldEncode.wireBytes <= activeWireBudget &&
                        heldEncode.wireBytes <= maxAllowedWire)
                    {
                        chosenEncode = heldEncode;
                        chosenWireBudget = activeWireBudget;
                        nextConsecutiveMisses = 0;
                    }
                    else if (activeScaleFeasible)
                    {
                        nextActive = activeScaleOpt.bestConfig;
                        chosenEncode = activeScaleOpt.encoded;
                        chosenWireBudget = activeScaleOpt.frameByteBudget;
                        configChanged = (nextActive.quality != priorActive.quality)
                            || (nextActive.fps != priorActive.fps);
                        changeReason = configChanged ? slowTrigger : QString();
                        nextConsecutiveMisses = 0;
                    }
                    else
                    {
                        frameDropped = true;
                        changeReason = QStringLiteral("frame_drop");
                    }
                }
            }
        }
    }
    else
    {
        // Fast Loop (per-frame): reuse m_activeConfig with bounded quality retries
        StreamOptimizer::OptimizerContext budgetCtx = buildOptimizerContext(
            cfgSnapshot, input, smoothedEncodeMs, priorWireBytes, cpuOverloaded, priorActive.fps);

        const qsizetype activeWireBudget = localOptimizer.computeMaxWireBytes(priorActive.fps, budgetCtx);
        const qsizetype activeJpegBudget =
            (activeWireBudget > overhead) ? (activeWireBudget - overhead) : 0;

        StreamOptimizer::FastContext fastCtx;
        fastCtx.maxWireBytes = activeWireBudget;
        fastCtx.maxJpegBytes = activeJpegBudget;
        fastCtx.minQuality = cfgSnapshot.minQuality;
        fastCtx.maxQuality = cfgSnapshot.maxQuality;
        fastCtx.maxRetries = cfgSnapshot.maxFastRetries;
        fastCtx.maxTimeMs = cfgSnapshot.maxFastLoopTimeMs;

        const EncodeAttemptResult fastRes =
            localOptimizer.encodeFast(input.frameId, input.frame, priorActive, fastCtx);

        if (fastRes.valid && fastRes.wireBytes <= activeWireBudget && fastRes.wireBytes <= maxAllowedWire)
        {
            chosenEncode = fastRes;
            chosenWireBudget = activeWireBudget;
            if (fastRes.encodeAttempts <= 1)
            {
                nextConsecutiveMisses = 0;
            }
            else if (fastRes.qualityUsed < priorActive.quality - cfgSnapshot.qualityHysteresisStep ||
                     fastRes.encodeAttempts > cfgSnapshot.maxFastRetries)
            {
                ++nextConsecutiveMisses;
            }
        }
        else
        {
            ++nextConsecutiveMisses;
            // Attempt 1 emergency downscale at minQuality before dropping
            double emergencyScale = cfgSnapshot.minScale;
            for (double s : localOptimizer.config().scaleRatios)
            {
                if (s < priorActive.scale - 1e-6 && s >= cfgSnapshot.minScale - 1e-6)
                {
                    emergencyScale = s;
                    break;
                }
            }

            bool recovered = false;
            if (emergencyScale < priorActive.scale - 1e-6)
            {
                const qsizetype fbWireBudget = localOptimizer.computeMaxWireBytes(priorActive.fps, budgetCtx);
                const qsizetype fbJpegBudget = (fbWireBudget > overhead) ? (fbWireBudget - overhead) : 0;
                const EncodeAttemptResult emergencyEnc = localOptimizer.encodeExact(
                    input.frameId, input.frame, emergencyScale, cfgSnapshot.minQuality, fbJpegBudget);
                if (emergencyEnc.valid && emergencyEnc.wireBytes <= fbWireBudget &&
                    emergencyEnc.wireBytes <= maxAllowedWire)
                {
                    nextActive = StreamConfig{
                        cfgSnapshot.minQuality,
                        emergencyScale,
                        priorActive.fps,
                        localOptimizer.computeObjectiveScore(
                            cfgSnapshot.minQuality, emergencyScale, priorActive.fps, budgetCtx),
                        emergencyEnc.wireBytes
                    };
                    nextResChangeMs = input.nowMs;
                    nextStableTicks = 0;
                    nextStableSinceMs = -1;
                    chosenEncode = emergencyEnc;
                    chosenWireBudget = fbWireBudget;
                    configChanged = true;
                    changeReason = QStringLiteral("emergency_drop");
                    recovered = true;
                }
            }

            if (!recovered)
            {
                const qsizetype minWireBudget = localOptimizer.computeMaxWireBytes(cfgSnapshot.minFps, budgetCtx);
                const qsizetype minJpegBudget = (minWireBudget > overhead) ? (minWireBudget - overhead) : 0;
                const EncodeAttemptResult lastResort = localOptimizer.encodeExact(
                    input.frameId, input.frame, cfgSnapshot.minScale, cfgSnapshot.minQuality, minJpegBudget);
                if (lastResort.valid && lastResort.wireBytes <= minWireBudget &&
                    lastResort.wireBytes <= maxAllowedWire)
                {
                    const bool scaleChangedNow = std::abs(cfgSnapshot.minScale - priorActive.scale) > 1e-6;
                    if (scaleChangedNow)
                    {
                        nextResChangeMs = input.nowMs;
                    }
                    nextActive = StreamConfig{
                        cfgSnapshot.minQuality,
                        cfgSnapshot.minScale,
                        cfgSnapshot.minFps,
                        localOptimizer.computeObjectiveScore(
                            cfgSnapshot.minQuality, cfgSnapshot.minScale, cfgSnapshot.minFps, budgetCtx),
                        lastResort.wireBytes
                    };
                    nextStableTicks = 0;
                    nextStableSinceMs = -1;
                    chosenEncode = lastResort;
                    chosenWireBudget = minWireBudget;
                    configChanged = true;
                    changeReason = QStringLiteral("emergency_drop");
                }
                else
                {
                    frameDropped = true;
                    chosenWireBudget = std::max<qsizetype>(0, activeWireBudget);
                    changeReason = QStringLiteral("frame_drop");
                }
            }
        }
    }

    // Strict zero-over-budget emission invariant
    if (!frameDropped)
    {
        if (!chosenEncode.valid ||
            chosenEncode.jpeg.isEmpty() ||
            chosenEncode.wireBytes <= overhead ||
            chosenEncode.wireBytes > maxAllowedWire ||
            (chosenWireBudget > 0 && chosenEncode.wireBytes > chosenWireBudget))
        {
            frameDropped = true;
            chosenEncode = EncodeAttemptResult{};
            changeReason = QStringLiteral("frame_drop");
        }
    }

    // Commit state back under mutex if not invalidated by reset()
    QMutexLocker locker(&m_mutex);
    if (entryEpoch != m_resetEpoch)
    {
        decision.shouldSend = false;
        decision.dropped = true;
        decision.configChangeReason = QStringLiteral("frame_drop");
        decision.nextFrameDelayMs = 1000 / std::max(1, cfgSnapshot.minFps);
        return decision;
    }

    // Merge localOptimizer caches if no reset occurred
    m_optimizer = localOptimizer;

    if (usedSlowLoop)
    {
        ++m_slowLoopCount;
        m_lastSlowLoopMs = input.nowMs;
        m_lastSlowLoopBps = std::max(1.0, finiteOr(input.availableBandwidthBps, 1.0));
        m_lastSlowLoopMotion = std::clamp(finiteOr(input.motionScore, 0.0), 0.0, 1.0);
        m_lastSlowLoopRttMs = rttForPacing;
        m_lastSlowLoopCongested = input.networkCongested;
    }
    else
    {
        ++m_fastLoopCount;
    }

    m_hasActiveConfig = nextHasActive;
    m_activeConfig = nextActive;
    m_lastResolutionChangeMs = nextResChangeMs;
    m_upgradeStableSinceMs = nextStableSinceMs;
    m_upgradeStableTicks = nextStableTicks;
    m_consecutiveFastMisses = nextConsecutiveMisses;

    if (chosenEncode.encodeTimeMs >= 0 && !frameDropped)
    {
        const double encMs = static_cast<double>(chosenEncode.encodeTimeMs);
        if (!m_hasEncodeSample)
        {
            m_smoothedEncodeTimeMs = encMs;
            m_hasEncodeSample = true;
        }
        else
        {
            m_smoothedEncodeTimeMs = 0.25 * encMs + 0.75 * m_smoothedEncodeTimeMs;
        }
    }

    if (frameDropped)
    {
        ++m_droppedFrames;
        decision.shouldSend = false;
        decision.dropped = true;
        decision.usedSlowLoop = usedSlowLoop;
        decision.configChanged = false;
        decision.wireBytes = 0;
        decision.frameByteBudget = chosenWireBudget;
        decision.activeConfig = m_activeConfig;
        decision.qualityUsed = 0;
        decision.scaleUsed = m_activeConfig.scale;
        decision.targetFps = m_activeConfig.fps > 0 ? m_activeConfig.fps : cfgSnapshot.minFps;
        decision.encodeRetryCount = std::max(0, chosenEncode.encodeAttempts - 1);
        decision.encodeTimeMs = chosenEncode.encodeTimeMs;
        decision.nextFrameDelayMs = 1000 / std::max(1, cfgSnapshot.minFps);
        decision.configChangeReason = QStringLiteral("frame_drop");
    }
    else
    {
        m_lastSentWireBytes = chosenEncode.wireBytes;
        m_sentTimestampsMs.push_back(input.nowMs);
        while (m_sentTimestampsMs.size() > 1 && (input.nowMs - m_sentTimestampsMs.front() > 2000))
        {
            m_sentTimestampsMs.pop_front();
        }

        decision.shouldSend = true;
        decision.dropped = false;
        decision.usedSlowLoop = usedSlowLoop;
        decision.configChanged = configChanged;
        decision.jpeg = chosenEncode.jpeg;
        decision.encodedSize = chosenEncode.encodedSize;
        decision.wireBytes = chosenEncode.wireBytes;
        decision.frameByteBudget = chosenWireBudget;
        decision.activeConfig = m_activeConfig;
        decision.qualityUsed = chosenEncode.qualityUsed;
        decision.scaleUsed = chosenEncode.scaleUsed;
        decision.targetFps = m_activeConfig.fps;
        decision.encodeRetryCount = std::max(0, chosenEncode.encodeAttempts - 1);
        decision.encodeTimeMs = chosenEncode.encodeTimeMs;
        decision.nextFrameDelayMs = computeNextFrameDelayMs(
            chosenEncode.wireBytes,
            chosenEncode.encodeTimeMs,
            rttForPacing,
            input.availableBandwidthBps,
            m_activeConfig.fps,
            input.ackEnabled);
        decision.configChangeReason = changeReason;
    }

    const double resolvedActualFps = (std::isfinite(input.actualFps) && input.actualFps >= 0.0)
        ? input.actualFps
        : computeActualFpsLocked(input.nowMs);

    decision.metricsSnapshot = StreamMetricsSnapshot{
        input.estimatedBandwidthBps,
        input.observedThroughputBps,
        decision.frameByteBudget,
        decision.wireBytes,
        std::clamp(finiteOr(input.motionScore, 0.0), 0.0, 1.0),
        decision.qualityUsed,
        decision.encodedSize,
        decision.targetFps,
        resolvedActualFps,
        decision.encodeTimeMs,
        decision.encodeRetryCount,
        input.ackLatencyMs,
        input.pendingBytes,
        m_droppedFrames,
        decision.configChangeReason
    };

    return decision;
}

AdaptiveStreamController::StateSnapshot AdaptiveStreamController::state() const
{
    QMutexLocker locker(&m_mutex);
    return StateSnapshot{
        m_hasActiveConfig,
        m_activeConfig,
        m_lastSlowLoopMs,
        m_lastResolutionChangeMs,
        m_upgradeStableSinceMs,
        m_upgradeStableTicks,
        m_consecutiveFastMisses,
        m_droppedFrames,
        m_slowLoopCount,
        m_fastLoopCount
    };
}

StreamConfig AdaptiveStreamController::activeConfig() const
{
    QMutexLocker locker(&m_mutex);
    return m_activeConfig;
}
