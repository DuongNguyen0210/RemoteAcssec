#include "Streaming/StreamOptimizer.h"

#include <QElapsedTimer>

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

StreamOptimizer::Config StreamOptimizer::sanitizeConfig(const Config &cfg)
{
    Config out = cfg;

    QVector<double> cleanScales;
    for (double s : cfg.scaleRatios)
    {
        if (std::isfinite(s) && s > 0.0 && s <= 1.0)
        {
            cleanScales.push_back(s);
        }
    }
    if (cleanScales.isEmpty())
    {
        cleanScales = {1.0, 0.85, 0.75, 0.67, 0.5};
    }
    std::sort(cleanScales.begin(), cleanScales.end(), std::greater<double>());
    QVector<double> dedupScales;
    for (double s : cleanScales)
    {
        if (dedupScales.isEmpty() || std::abs(dedupScales.back() - s) > 1e-6)
        {
            dedupScales.push_back(s);
        }
    }
    out.scaleRatios = std::move(dedupScales);

    QVector<int> cleanFps;
    for (int f : cfg.fpsLevels)
    {
        if (f >= 1 && f <= 120)
        {
            cleanFps.push_back(f);
        }
    }
    if (cleanFps.isEmpty())
    {
        cleanFps = {2, 3, 5, 8, 10, 15};
    }
    std::sort(cleanFps.begin(), cleanFps.end());
    cleanFps.erase(std::unique(cleanFps.begin(), cleanFps.end()), cleanFps.end());
    out.fpsLevels = std::move(cleanFps);

    out.minQuality = std::clamp(cfg.minQuality, ScreenEncoder::MIN_QUALITY, ScreenEncoder::MAX_QUALITY);
    out.maxQuality = std::clamp(cfg.maxQuality, out.minQuality, ScreenEncoder::MAX_QUALITY);
    out.headerOverheadBytes = std::clamp<qsizetype>(
        cfg.headerOverheadBytes, WIRE_OVERHEAD_BYTES, DEFAULT_MAX_WIRE_FRAME_BYTES - 1);

    const qsizetype protocolMaxWire =
        static_cast<qsizetype>(ScreenFramePacketizer::maxChunkData()) + out.headerOverheadBytes;
    const qsizetype relaySafeMaxWire = DEFAULT_MAX_WIRE_FRAME_BYTES - WIRE_OVERHEAD_BYTES + out.headerOverheadBytes;
    const qsizetype maxAllowedWire = std::min(protocolMaxWire, relaySafeMaxWire);
    out.maxWireFrameBytes = (cfg.maxWireFrameBytes <= out.headerOverheadBytes)
        ? 0
        : std::min(cfg.maxWireFrameBytes, maxAllowedWire);

    out.processingMarginMs = std::max(0.0, finiteOr(cfg.processingMarginMs, 8.0));
    out.highRttFallbackMaxTxMs = std::max(10.0, finiteOr(cfg.highRttFallbackMaxTxMs, 400.0));
    out.maxResizeCacheEntries = std::clamp(cfg.maxResizeCacheEntries, 1, 256);
    out.maxEncodeCacheEntries = std::clamp(cfg.maxEncodeCacheEntries, 1, 1024);

    return out;
}

void StreamOptimizer::normalizeWeights(double wQ,
                                       double wR,
                                       double wP,
                                       double &outWQ,
                                       double &outWR,
                                       double &outWP)
{
    const double q = std::max(0.0, finiteOr(wQ, 0.0));
    const double r = std::max(0.0, finiteOr(wR, 0.0));
    const double p = std::max(0.0, finiteOr(wP, 0.0));
    const double sum = q + r + p;
    if (sum <= 0.0)
    {
        outWQ = 0.35;
        outWR = 0.50;
        outWP = 0.15;
        return;
    }
    outWQ = q / sum;
    outWR = r / sum;
    outWP = p / sum;
}

StreamOptimizer::StreamOptimizer()
    : StreamOptimizer(Config{})
{
}

StreamOptimizer::StreamOptimizer(const Config &config)
    : m_config(sanitizeConfig(config)),
      m_encodeFn([](const QImage &scaledImage, int quality) {
          return ScreenEncoder::encodeAt(scaledImage, quality);
      })
{
}

void StreamOptimizer::setConfig(const Config &config)
{
    m_config = sanitizeConfig(config);
    clearCaches();
}

StreamOptimizer::Config StreamOptimizer::config() const
{
    return m_config;
}

void StreamOptimizer::setEncodeFunction(EncodeFunction fn)
{
    if (fn)
    {
        m_encodeFn = std::move(fn);
    }
    else
    {
        m_encodeFn = [](const QImage &scaledImage, int quality) {
            return ScreenEncoder::encodeAt(scaledImage, quality);
        };
    }
    clearCaches();
}

void StreamOptimizer::clearCaches()
{
    m_resizeCache.clear();
    m_resizeOrder.clear();
    m_encodeCache.clear();
    m_encodeOrder.clear();
}

qsizetype StreamOptimizer::effectiveMaxPayload() const
{
    if (m_config.maxWireFrameBytes <= m_config.headerOverheadBytes)
    {
        return 0;
    }
    const qsizetype protocolChunkCap = static_cast<qsizetype>(ScreenFramePacketizer::maxChunkData());
    const qsizetype relaySinglePacketCap = DEFAULT_MAX_WIRE_FRAME_BYTES - WIRE_OVERHEAD_BYTES;
    const qsizetype configuredCap = m_config.maxWireFrameBytes - m_config.headerOverheadBytes;
    return std::max<qsizetype>(0, std::min({configuredCap, relaySinglePacketCap, protocolChunkCap}));
}

qsizetype StreamOptimizer::effectiveMaxWireBytes() const
{
    const qsizetype payload = effectiveMaxPayload();
    return (payload > 0) ? (payload + m_config.headerOverheadBytes) : 0;
}

double StreamOptimizer::extractBaseRttMs(const OptimizerContext &ctx) const
{
    const double smoothed = (std::isfinite(ctx.smoothedRttMs) && ctx.smoothedRttMs > 0.0)
        ? ctx.smoothedRttMs
        : 0.0;
    const double minRtt = (std::isfinite(ctx.minRttMs) && ctx.minRttMs > 0.0)
        ? ctx.minRttMs
        : 0.0;
    const double bwForTx = (std::isfinite(ctx.estimatedBandwidthBps) && ctx.estimatedBandwidthBps > 0.0)
        ? ctx.estimatedBandwidthBps
        : ((std::isfinite(ctx.availableBandwidthBps) && ctx.availableBandwidthBps > 0.0)
               ? ctx.availableBandwidthBps
               : 0.0);

    if (ctx.priorWireBytes > 0 && smoothed > 0.0 && bwForTx > 0.0)
    {
        const double priorTxMs = (static_cast<double>(ctx.priorWireBytes) * 1000.0) / bwForTx;
        if (priorTxMs < smoothed)
        {
            return smoothed - priorTxMs;
        }
        if (minRtt > 0.0)
        {
            return std::min(smoothed, minRtt);
        }
        return 0.0;
    }

    if (minRtt > 0.0)
    {
        return minRtt;
    }
    return smoothed;
}

qsizetype StreamOptimizer::computeMaxWireBytes(int fps, const OptimizerContext &ctx) const
{
    const qsizetype protocolWireCap = effectiveMaxWireBytes();
    if (protocolWireCap <= m_config.headerOverheadBytes)
    {
        return 0;
    }

    if (!std::isfinite(ctx.availableBandwidthBps) || ctx.availableBandwidthBps <= 0.0)
    {
        return 0;
    }

    const int safeFps = std::max(1, fps);
    const double safeBps = ctx.availableBandwidthBps;
    const double maxWireDouble = static_cast<double>(protocolWireCap);

    const qsizetype bwWireCap = static_cast<qsizetype>(
        std::min(maxWireDouble, std::max(0.0, std::floor(safeBps / static_cast<double>(safeFps)))));
    qsizetype cap = std::min(protocolWireCap, bwWireCap);

    const double frameIntervalMs = 1000.0 / static_cast<double>(safeFps);
    const double encodeMs = (std::isfinite(ctx.estimatedEncodeTimeMs) && ctx.estimatedEncodeTimeMs > 0.0)
        ? ctx.estimatedEncodeTimeMs
        : 0.0;
    const double baseRttMs = extractBaseRttMs(ctx);
    const double availableTxMs = frameIntervalMs - encodeMs - baseRttMs - m_config.processingMarginMs;

    if (availableTxMs > 1.0)
    {
        const qsizetype latencyWireCap = static_cast<qsizetype>(
            std::min(maxWireDouble, std::max(0.0, std::floor((safeBps * availableTxMs) / 1000.0))));
        cap = std::min(cap, latencyWireCap);
    }
    else if (ctx.allowHighRttFallback && safeFps <= std::max(1, ctx.minFps))
    {
        const qsizetype highRttCap = static_cast<qsizetype>(
            std::min(maxWireDouble, std::max(0.0, std::floor((safeBps * m_config.highRttFallbackMaxTxMs) / 1000.0))));
        cap = std::min(cap, highRttCap);
    }
    else
    {
        return 0;
    }

    if (ctx.minRecoveryWireBytes > 0 && safeFps <= std::max(1, ctx.minFps))
    {
        if (bwWireCap >= ctx.minRecoveryWireBytes && cap < ctx.minRecoveryWireBytes)
        {
            const qsizetype recoveryTarget =
                std::min({protocolWireCap, bwWireCap, ctx.minRecoveryWireBytes + 512});
            cap = std::max(cap, recoveryTarget);
        }
    }

    if (cap <= m_config.headerOverheadBytes)
    {
        return 0;
    }
    return cap;
}

qsizetype StreamOptimizer::computeMaxBytes(int fps, const OptimizerContext &ctx) const
{
    const qsizetype wireCap = computeMaxWireBytes(fps, ctx);
    if (wireCap <= m_config.headerOverheadBytes)
    {
        return 0;
    }
    return wireCap - m_config.headerOverheadBytes;
}

int StreamOptimizer::maxFeasibleFpsForWireBytes(qsizetype actualWireBytes,
                                                const OptimizerContext &ctx) const
{
    const qsizetype protocolWireCap = effectiveMaxWireBytes();
    if (actualWireBytes <= m_config.headerOverheadBytes || actualWireBytes > protocolWireCap)
    {
        return 0;
    }

    const int minBoundFps = std::clamp(ctx.minFps, 1, 120);
    const int maxBoundFps = std::clamp(ctx.maxFps, minBoundFps, 120);

    bool hasRecoveryFps = false;
    if (ctx.minRecoveryWireBytes > 0 &&
        std::isfinite(ctx.availableBandwidthBps) &&
        ctx.availableBandwidthBps / static_cast<double>(minBoundFps) >=
            static_cast<double>(ctx.minRecoveryWireBytes))
    {
        for (int p : m_config.fpsLevels)
        {
            if (p >= minBoundFps && p <= maxBoundFps &&
                computeMaxWireBytes(p, ctx) >= ctx.minRecoveryWireBytes)
            {
                hasRecoveryFps = true;
                break;
            }
        }
    }

    int bestFps = 0;
    for (int p : m_config.fpsLevels)
    {
        if (p < minBoundFps || p > maxBoundFps)
        {
            continue;
        }
        const qsizetype cap = computeMaxWireBytes(p, ctx);
        if (cap <= m_config.headerOverheadBytes)
        {
            continue;
        }
        if (hasRecoveryFps && cap < ctx.minRecoveryWireBytes)
        {
            continue;
        }
        if (actualWireBytes <= cap)
        {
            bestFps = std::max(bestFps, p);
        }
    }

    if (bestFps == 0 && minBoundFps > 0)
    {
        const qsizetype fallbackCap = computeMaxWireBytes(minBoundFps, ctx);
        if (fallbackCap > m_config.headerOverheadBytes && actualWireBytes <= fallbackCap)
        {
            bestFps = minBoundFps;
        }
    }

    return bestFps;
}

double StreamOptimizer::computeObjectiveScore(int quality,
                                              double scale,
                                              int fps,
                                              const OptimizerContext &ctx) const
{
    double nQ = 0.0;
    double nR = 0.0;
    double nP = 0.0;
    normalizeWeights(ctx.wQ, ctx.wR, ctx.wP, nQ, nR, nP);

    int maxDomainFps = std::max(1, ctx.maxFps);
    int domainTop = 0;
    for (int f : m_config.fpsLevels)
    {
        if (f <= maxDomainFps)
        {
            domainTop = std::max(domainTop, f);
        }
    }
    const int fpsDenom = std::max(1, domainTop > 0 ? domainTop : maxDomainFps);

    const double qNorm = std::clamp(static_cast<double>(quality) / 100.0, 0.0, 1.0);
    const double rNorm = std::clamp(finiteOr(scale, 0.0), 0.0, 1.0);
    const double pNorm = std::clamp(static_cast<double>(fps) / static_cast<double>(fpsDenom), 0.0, 1.0);
    const double scaleBias = std::max(0.0, finiteOr(ctx.lowMotionScaleBias, 0.0)) * rNorm;

    return (nQ * qNorm) + (nR * rNorm) + (nP * pNorm) + scaleBias;
}

int StreamOptimizer::resolveScaleIndex(double scale) const
{
    for (int i = 0; i < m_config.scaleRatios.size(); ++i)
    {
        if (std::abs(m_config.scaleRatios[i] - scale) <= 1e-6)
        {
            return i;
        }
    }
    return static_cast<int>(std::lround(scale * 10000.0)) + 1000;
}

QImage StreamOptimizer::getOrResizeImage(quint64 frameId,
                                         const QImage &source,
                                         int scaleIndex,
                                         double scale,
                                         int &resizeCalls)
{
    const ResizeCacheKey key{frameId, scaleIndex};
    const qint64 srcKey = source.cacheKey();
    const QSize srcSize = source.size();

    const auto it = m_resizeCache.constFind(key);
    if (it != m_resizeCache.constEnd() &&
        it->sourceCacheKey == srcKey &&
        it->sourceSize == srcSize &&
        !it->image.isNull())
    {
        return it->image;
    }

    QImage resized = ScreenEncoder::scaleImage(source, scale, Qt::SmoothTransformation);
    ++resizeCalls;

    if (!resized.isNull())
    {
        if (!m_resizeCache.contains(key))
        {
            while (static_cast<int>(m_resizeOrder.size()) >= m_config.maxResizeCacheEntries)
            {
                const ResizeCacheKey oldest = m_resizeOrder.front();
                m_resizeOrder.pop_front();
                m_resizeCache.remove(oldest);
            }
            m_resizeOrder.push_back(key);
        }
        m_resizeCache.insert(key, ResizeCacheEntry{resized, srcKey, srcSize});
    }
    return resized;
}

TimedEncodedFrame StreamOptimizer::getOrEncodeFrame(quint64 frameId,
                                                    const QImage &source,
                                                    int scaleIndex,
                                                    double scale,
                                                    int quality,
                                                    int &resizeCalls,
                                                    int &encodeCalls,
                                                    int &cacheHits)
{
    const int clampedQ = std::clamp(quality, ScreenEncoder::MIN_QUALITY, ScreenEncoder::MAX_QUALITY);
    const EncodeCacheKey key{frameId, scaleIndex, clampedQ};
    const qint64 srcKey = source.cacheKey();
    const QSize srcSize = source.size();

    const auto it = m_encodeCache.constFind(key);
    if (it != m_encodeCache.constEnd() &&
        it->sourceCacheKey == srcKey &&
        it->sourceSize == srcSize)
    {
        ++cacheHits;
        return it->frame;
    }

    const QImage scaled = getOrResizeImage(frameId, source, scaleIndex, scale, resizeCalls);
    if (scaled.isNull())
    {
        return TimedEncodedFrame{};
    }

    TimedEncodedFrame encoded = m_encodeFn(scaled, clampedQ);
    ++encodeCalls;

    if (!m_encodeCache.contains(key))
    {
        while (static_cast<int>(m_encodeOrder.size()) >= m_config.maxEncodeCacheEntries)
        {
            const EncodeCacheKey oldest = m_encodeOrder.front();
            m_encodeOrder.pop_front();
            m_encodeCache.remove(oldest);
        }
        m_encodeOrder.push_back(key);
    }
    m_encodeCache.insert(key, EncodeCacheEntry{encoded, srcKey, srcSize});
    return encoded;
}

StreamOptimizer::OptimizeResult StreamOptimizer::optimizeFull(quint64 frameId,
                                                              const QImage &frame,
                                                              const OptimizerContext &ctx)
{
    OptimizeResult out;
    out.shouldDropFrame = true;

    if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0)
    {
        return out;
    }

    const qsizetype maxPayload = effectiveMaxPayload();
    if (maxPayload <= 0)
    {
        return out;
    }

    const int minQ = std::clamp(ctx.minQuality, m_config.minQuality, m_config.maxQuality);
    const int maxQ = std::clamp(ctx.maxQuality, minQ, m_config.maxQuality);
    const double minScaleBound = std::clamp(finiteOr(ctx.minScale, 0.5), 0.05, 1.0);
    const double maxScaleBound = std::clamp(finiteOr(ctx.maxScale, 1.0), minScaleBound, 1.0);
    const int minFpsBound = std::clamp(ctx.minFps, 1, 120);
    const int maxFpsBound = std::clamp(ctx.maxFps, minFpsBound, 120);

    struct ScaleEntry
    {
        int index = 0;
        double scale = 1.0;
    };
    QVector<ScaleEntry> candidateScales;
    for (int i = 0; i < m_config.scaleRatios.size(); ++i)
    {
        const double s = m_config.scaleRatios[i];
        if (s + 1e-6 >= minScaleBound && s - 1e-6 <= maxScaleBound)
        {
            candidateScales.push_back(ScaleEntry{i, s});
        }
    }
    if (candidateScales.isEmpty())
    {
        candidateScales.push_back(ScaleEntry{resolveScaleIndex(minScaleBound), minScaleBound});
    }

    struct FpsBudget
    {
        int fps = 0;
        qsizetype maxWire = 0;
        qsizetype maxJpeg = 0;
    };
    QVector<FpsBudget> candidateFps;
    for (int f : m_config.fpsLevels)
    {
        if (f < minFpsBound || f > maxFpsBound)
        {
            continue;
        }
        const qsizetype w = computeMaxWireBytes(f, ctx);
        const qsizetype j = (w > m_config.headerOverheadBytes) ? (w - m_config.headerOverheadBytes) : 0;
        if (j > 0)
        {
            candidateFps.push_back(FpsBudget{f, w, std::min(j, maxPayload)});
        }
    }
    if (candidateFps.isEmpty())
    {
        const qsizetype w = computeMaxWireBytes(minFpsBound, ctx);
        const qsizetype j = (w > m_config.headerOverheadBytes) ? (w - m_config.headerOverheadBytes) : 0;
        if (j > 0)
        {
            candidateFps.push_back(FpsBudget{minFpsBound, w, std::min(j, maxPayload)});
        }
    }
    if (candidateFps.isEmpty())
    {
        return out;
    }

    if (ctx.minRecoveryWireBytes > 0 &&
        std::isfinite(ctx.availableBandwidthBps) &&
        ctx.availableBandwidthBps / static_cast<double>(minFpsBound) >=
            static_cast<double>(ctx.minRecoveryWireBytes))
    {
        QVector<FpsBudget> recoveryFps;
        for (const auto &fb : candidateFps)
        {
            if (fb.maxWire >= ctx.minRecoveryWireBytes)
            {
                recoveryFps.push_back(fb);
            }
        }
        if (!recoveryFps.isEmpty())
        {
            candidateFps = std::move(recoveryFps);
        }
    }

    const qsizetype largestJpegBudget = candidateFps.front().maxJpeg;
    const int highestFeasibleFps = candidateFps.back().fps;

    bool hasBest = false;
    double bestScore = -1.0;

    auto considerCandidate = [&](int q, double s, const TimedEncodedFrame &enc) {
        if (!enc.valid || enc.jpeg.isEmpty() || enc.jpeg.size() > maxPayload)
        {
            return 0;
        }
        const qsizetype wireBytes = enc.jpeg.size() + m_config.headerOverheadBytes;
        const int feasibleFps = maxFeasibleFpsForWireBytes(wireBytes, ctx);
        if (feasibleFps <= 0)
        {
            return 0;
        }
        const qsizetype fpsMaxWire = computeMaxWireBytes(feasibleFps, ctx);
        if (wireBytes > fpsMaxWire || wireBytes > effectiveMaxWireBytes())
        {
            return 0;
        }

        const double score = computeObjectiveScore(q, s, feasibleFps, ctx);
        const bool better = !hasBest
            || (score > bestScore + 1e-12)
            || (std::abs(score - bestScore) <= 1e-12 &&
                (s > out.bestConfig.scale + 1e-6 ||
                 (std::abs(s - out.bestConfig.scale) <= 1e-6 && q > out.bestConfig.quality)));
        if (better)
        {
            hasBest = true;
            bestScore = score;
            out.bestConfig = StreamConfig{q, s, feasibleFps, score, wireBytes};
            out.encoded = EncodeAttemptResult{
                enc.jpeg,
                enc.size,
                q,
                s,
                wireBytes,
                0,
                enc.encodeTimeMs,
                true
            };
            out.frameByteBudget = fpsMaxWire;
            out.jpegByteBudget = fpsMaxWire - m_config.headerOverheadBytes;
            out.feasible = true;
            out.shouldDropFrame = false;
        }
        return feasibleFps;
    };

    for (const ScaleEntry &sc : candidateScales)
    {
        if (hasBest)
        {
            const double upperScore = computeObjectiveScore(maxQ, sc.scale, highestFeasibleFps, ctx);
            if (upperScore <= bestScore + 1e-12)
            {
                continue;
            }
        }

        struct ProbedPoint
        {
            int q = 0;
            qsizetype jpegBytes = 0;
        };
        QVector<ProbedPoint> probedForScale;

        auto probeQ = [&](int q) -> TimedEncodedFrame {
            const TimedEncodedFrame enc = getOrEncodeFrame(
                frameId,
                frame,
                sc.index,
                sc.scale,
                q,
                out.resizeCalls,
                out.totalEncodeCalls,
                out.encodeCacheHits);
            const qsizetype sz = (enc.valid && !enc.jpeg.isEmpty()) ? enc.jpeg.size() : -1;
            probedForScale.push_back(ProbedPoint{q, sz});
            return enc;
        };

        const TimedEncodedFrame minEnc = probeQ(minQ);
        if (!minEnc.valid || minEnc.jpeg.isEmpty() || minEnc.jpeg.size() > largestJpegBudget)
        {
            continue;
        }
        const int fpsAtMinQ = considerCandidate(minQ, sc.scale, minEnc);

        int fpsAtMaxQ = 0;
        if (maxQ > minQ)
        {
            const TimedEncodedFrame maxEnc = probeQ(maxQ);
            fpsAtMaxQ = considerCandidate(maxQ, sc.scale, maxEnc);
            if (fpsAtMaxQ >= highestFeasibleFps || (fpsAtMinQ > 0 && fpsAtMaxQ == fpsAtMinQ))
            {
                continue;
            }
        }

        if (ctx.warmStartQuality >= minQ && ctx.warmStartQuality <= maxQ)
        {
            const bool alreadyProbed = std::any_of(
                probedForScale.cbegin(), probedForScale.cend(),
                [&](const ProbedPoint &pt) { return pt.q == ctx.warmStartQuality; });
            if (!alreadyProbed)
            {
                const TimedEncodedFrame warmEnc = probeQ(ctx.warmStartQuality);
                considerCandidate(ctx.warmStartQuality, sc.scale, warmEnc);
            }
        }

        for (int idx = candidateFps.size() - 1; idx >= 0; --idx)
        {
            const FpsBudget &fb = candidateFps[idx];
            if (fb.fps <= fpsAtMaxQ)
            {
                break;
            }
            if (hasBest &&
                computeObjectiveScore(maxQ, sc.scale, fb.fps, ctx) <= bestScore + 1e-12)
            {
                continue;
            }

            int lo = minQ;
            int hi = maxQ;
            for (const ProbedPoint &pt : probedForScale)
            {
                if (pt.jpegBytes > 0 && pt.jpegBytes <= fb.maxJpeg)
                {
                    lo = std::max(lo, pt.q + 1);
                }
                else
                {
                    hi = std::min(hi, pt.q - 1);
                }
            }

            while (lo <= hi)
            {
                const int mid = lo + (hi - lo) / 2;
                bool foundCached = false;
                qsizetype cachedSize = -1;
                for (const ProbedPoint &pt : probedForScale)
                {
                    if (pt.q == mid)
                    {
                        foundCached = true;
                        cachedSize = pt.jpegBytes;
                        break;
                    }
                }

                if (!foundCached)
                {
                    const TimedEncodedFrame midEnc = probeQ(mid);
                    cachedSize = (midEnc.valid && !midEnc.jpeg.isEmpty()) ? midEnc.jpeg.size() : -1;
                    if (cachedSize > 0)
                    {
                        considerCandidate(mid, sc.scale, midEnc);
                    }
                }
                else if (cachedSize > 0 && cachedSize <= fb.maxJpeg)
                {
                    const TimedEncodedFrame cachedEnc = getOrEncodeFrame(
                        frameId,
                        frame,
                        sc.index,
                        sc.scale,
                        mid,
                        out.resizeCalls,
                        out.totalEncodeCalls,
                        out.encodeCacheHits);
                    considerCandidate(mid, sc.scale, cachedEnc);
                }

                if (cachedSize > 0 && cachedSize <= fb.maxJpeg)
                {
                    lo = mid + 1;
                }
                else
                {
                    hi = mid - 1;
                }
            }
        }
    }

    if (out.feasible)
    {
        out.encoded.encodeAttempts = out.totalEncodeCalls;
    }
    return out;
}

EncodeAttemptResult StreamOptimizer::encodeFast(quint64 frameId,
                                                const QImage &frame,
                                                const StreamConfig &active,
                                                const FastContext &ctx)
{
    EncodeAttemptResult out;
    out.scaleUsed = active.scale;

    if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0)
    {
        return out;
    }

    const qsizetype maxPayload = effectiveMaxPayload();
    if (maxPayload <= 0)
    {
        return out;
    }

    qsizetype budgetJpeg = 0;
    if (ctx.maxJpegBytes > 0)
    {
        budgetJpeg = std::min(ctx.maxJpegBytes, maxPayload);
    }
    else if (ctx.maxWireBytes > m_config.headerOverheadBytes)
    {
        budgetJpeg = std::min(ctx.maxWireBytes - m_config.headerOverheadBytes, maxPayload);
    }
    if (budgetJpeg <= 0)
    {
        return out;
    }

    const int minQ = std::clamp(ctx.minQuality, m_config.minQuality, m_config.maxQuality);
    const int maxQ = std::clamp(ctx.maxQuality, minQ, m_config.maxQuality);
    const int startQ = std::clamp(active.quality, minQ, maxQ);
    const int maxRetries = std::clamp(ctx.maxRetries, 1, 4);
    const qint64 maxTimeMs = std::max<qint64>(1, ctx.maxTimeMs);
    const int scaleIndex = resolveScaleIndex(active.scale);

    QElapsedTimer fastTimer;
    fastTimer.start();

    int resizeCalls = 0;
    int encodeCalls = 0;
    int cacheHits = 0;
    int attempts = 0;
    qint64 cumulativeEncodeMs = 0;

    auto tryQuality = [&](int q) -> TimedEncodedFrame {
        const TimedEncodedFrame enc = getOrEncodeFrame(
            frameId,
            frame,
            scaleIndex,
            active.scale,
            q,
            resizeCalls,
            encodeCalls,
            cacheHits);
        ++attempts;
        cumulativeEncodeMs += std::max<qint64>(0, enc.encodeTimeMs);
        return enc;
    };

    EncodeAttemptResult bestValid;
    auto acceptIfValid = [&](int q, const TimedEncodedFrame &enc) -> bool {
        if (!enc.valid || enc.jpeg.isEmpty() || enc.jpeg.size() > budgetJpeg || enc.jpeg.size() > maxPayload)
        {
            return false;
        }
        const qsizetype wireBytes = enc.jpeg.size() + m_config.headerOverheadBytes;
        if (!bestValid.valid || q > bestValid.qualityUsed)
        {
            bestValid.jpeg = enc.jpeg;
            bestValid.encodedSize = enc.size;
            bestValid.qualityUsed = q;
            bestValid.scaleUsed = active.scale;
            bestValid.wireBytes = wireBytes;
            bestValid.valid = true;
        }
        return true;
    };

    const TimedEncodedFrame first = tryQuality(startQ);
    if (acceptIfValid(startQ, first))
    {
        bestValid.encodeAttempts = attempts;
        bestValid.encodeTimeMs = std::max(cumulativeEncodeMs, fastTimer.elapsed());
        return bestValid;
    }

    if (startQ > minQ)
    {
        int lo = minQ;
        int hi = startQ - 1;
        for (int retry = 0; retry < maxRetries && lo <= hi; ++retry)
        {
            if (fastTimer.elapsed() >= maxTimeMs)
            {
                break;
            }

            int probeQ = lo + (hi - lo) / 2;
            if (!bestValid.valid && retry == maxRetries - 1)
            {
                probeQ = lo;
            }

            const TimedEncodedFrame candidate = tryQuality(probeQ);
            if (acceptIfValid(probeQ, candidate))
            {
                lo = probeQ + 1;
            }
            else
            {
                hi = probeQ - 1;
            }
        }
    }

    if (bestValid.valid)
    {
        bestValid.encodeAttempts = attempts;
        bestValid.encodeTimeMs = std::max(cumulativeEncodeMs, fastTimer.elapsed());
        return bestValid;
    }

    out.encodeAttempts = attempts;
    out.encodeTimeMs = std::max(cumulativeEncodeMs, fastTimer.elapsed());
    return out;
}

EncodeAttemptResult StreamOptimizer::encodeExact(quint64 frameId,
                                                 const QImage &frame,
                                                 double scale,
                                                 int quality,
                                                 qsizetype maxJpegBytes)
{
    EncodeAttemptResult out;
    out.scaleUsed = scale;

    if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0)
    {
        return out;
    }

    const qsizetype maxPayload = effectiveMaxPayload();
    const qsizetype cap = (maxJpegBytes > 0) ? std::min(maxJpegBytes, maxPayload) : maxPayload;
    if (cap <= 0)
    {
        return out;
    }

    const int clampedQ = std::clamp(quality, ScreenEncoder::MIN_QUALITY, ScreenEncoder::MAX_QUALITY);
    const int scaleIndex = resolveScaleIndex(scale);
    int resizeCalls = 0;
    int encodeCalls = 0;
    int cacheHits = 0;

    const TimedEncodedFrame enc = getOrEncodeFrame(
        frameId, frame, scaleIndex, scale, clampedQ, resizeCalls, encodeCalls, cacheHits);
    if (!enc.valid || enc.jpeg.isEmpty() || enc.jpeg.size() > cap || enc.jpeg.size() > maxPayload)
    {
        out.encodeTimeMs = enc.encodeTimeMs;
        out.encodeAttempts = 1;
        return out;
    }

    out.jpeg = enc.jpeg;
    out.encodedSize = enc.size;
    out.qualityUsed = clampedQ;
    out.scaleUsed = scale;
    out.wireBytes = enc.jpeg.size() + m_config.headerOverheadBytes;
    out.encodeAttempts = 1;
    out.encodeTimeMs = enc.encodeTimeMs;
    out.valid = true;
    return out;
}
