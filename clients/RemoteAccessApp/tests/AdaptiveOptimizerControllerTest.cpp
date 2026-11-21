#include <QtTest>

#include "Network/Protocol/ScreenFramePacketizer.h"
#include "Streaming/AdaptiveStreamController.h"
#include "Streaming/BandwidthEstimator.h"
#include "Streaming/MotionDetector.h"
#include "Streaming/ScreenEncoder.h"
#include "Streaming/StreamMetrics.h"
#include "Streaming/StreamOptimizer.h"

#include <QColor>
#include <QImage>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStringList>
#include <QtGlobal>

#include <cmath>
#include <limits>
#include <map>

class AdaptiveOptimizerControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void screenEncoderHelpersPreserveCompatAndScaleCorrectly();
    void streamOptimizerEnforcesEffectivePayloadLimitAndPacketizes();
    void streamOptimizerBinarySearchHandlesNonMonotonicJpegAndRetainsBestBuffer();
    void streamOptimizerCachesResizesAndComputesFpsAnalytically();
    void controllerFastLoopReusesConfigWithSingleEncodeWhenSteady();
    void controllerFastLoopBoundedRetriesAndSafeDropOnImpossibleBudget();
    void controllerSlowLoopTriggersOnPeriodicAndAllEvents();
    void controllerHysteresisFastDowngradeVsSlowUpgrade();
    void controllerResolutionCooldownAndEmergencyBypass();
    void controllerMinScoreImprovementPreventsOscillation();
    void controllerLowMotionTextReadabilityBiasAndHighMotionFps();
    void controllerFloorRecoveryMitigatesSub4096Trap();
    void controllerMinBoundsHighRttSurvivalAndSafeFallback();
    void controllerNextFrameDelayAvoidsRttDoubleCounting();
    void controllerPopulatesAll15MetricsFieldsWithShortTokens();
    void controllerResetClearsStateCleanly();
    void streamOptimizerRetainsCrossFpsProbedMidpointCandidates();
};

namespace
{
QImage makeNoisyFrame(const QSize &size, quint32 seed = 12345U)
{
    QImage img(size, QImage::Format_RGB32);
    QRandomGenerator rng(seed);
    for (int y = 0; y < size.height(); ++y)
    {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < size.width(); ++x)
        {
            line[x] = 0xff000000u | rng.generate();
        }
    }
    return img;
}

QImage makeTextLikeFrame(const QSize &size)
{
    QImage img(size, QImage::Format_RGB32);
    img.fill(Qt::white);
    for (int y = 8; y < size.height() - 8; y += 14)
    {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        QRgb *line2 = reinterpret_cast<QRgb *>(img.scanLine(y + 2));
        for (int x = 16; x < size.width() - 16; ++x)
        {
            const bool ink = ((x / 3) % 5 != 0) && ((x / 29) % 4 != 0);
            if (ink)
            {
                line[x] = qRgb(20, 20, 20);
                line2[x] = qRgb(40, 40, 40);
            }
        }
    }
    return img;
}

TimedEncodedFrame makeSyntheticTimedFrame(const QSize &size, int quality, qsizetype bytes, qint64 encodeMs = 2)
{
    QByteArray payload(bytes, static_cast<char>('A' + (quality % 26)));
    return TimedEncodedFrame{std::move(payload), size, quality, encodeMs, bytes > 0};
}

AdaptiveStreamController::FrameInput makeBaseInput(quint64 frameId,
                                                   const QImage &frame,
                                                   qint64 nowMs,
                                                   double availableBps,
                                                   double motionScore = 0.0)
{
    AdaptiveStreamController::FrameInput in;
    in.frameId = frameId;
    in.frame = frame;
    in.nowMs = nowMs;
    in.motionScore = motionScore;
    in.wQ = std::max(0.0, 0.35 - 0.15 * motionScore);
    in.wR = std::max(0.0, 0.50 - 0.25 * motionScore);
    in.wP = std::max(0.0, 0.15 + 0.40 * motionScore);
    in.availableBandwidthBps = availableBps;
    in.estimatedBandwidthBps = availableBps / 0.8;
    in.observedThroughputBps = availableBps * 0.9;
    in.smoothedRttMs = 20.0;
    in.minRttMs = 15.0;
    in.ackLatencyMs = 20.0;
    in.ackEnabled = true;
    return in;
}
} // namespace

void AdaptiveOptimizerControllerTest::screenEncoderHelpersPreserveCompatAndScaleCorrectly()
{
    QCOMPARE(ScreenEncoder::DEFAULT_QUALITY, 60);
    QCOMPARE(ScreenEncoder::MIN_QUALITY, 1);
    QCOMPARE(ScreenEncoder::MAX_QUALITY, 100);

    // Invalid / null guards
    QCOMPARE(ScreenEncoder::scaledDimensions(QSize(), 0.5), QSize());
    QCOMPARE(ScreenEncoder::scaledDimensions(QSize(1920, 1080), 0.0), QSize());
    QCOMPARE(ScreenEncoder::scaledDimensions(QSize(1920, 1080), -0.5), QSize());
    QCOMPARE(ScreenEncoder::scaledDimensions(QSize(1920, 1080), std::numeric_limits<double>::quiet_NaN()), QSize());
    QCOMPARE(ScreenEncoder::scaledDimensions(QSize(1920, 1080), std::numeric_limits<double>::infinity()), QSize());
    QVERIFY(ScreenEncoder::scaleImage(QImage(), 0.5).isNull());
    QVERIFY(!ScreenEncoder::encodeAt(QImage(), 70).valid);
    QVERIFY(!ScreenEncoder::encodeScaledAt(QImage(), 0.5, 70).valid);

    // Minimum 1x1 clamp
    QCOMPARE(ScreenEncoder::scaledDimensions(QSize(1, 1), 0.1), QSize(1, 1));
    QCOMPARE(ScreenEncoder::scaledDimensions(QSize(1920, 1080), 0.5), QSize(960, 540));
    QCOMPARE(ScreenEncoder::scaledDimensions(QSize(1920, 1080), 1.5), QSize(1920, 1080));

    // Scale = 1.0 returns matching image size; smaller scale resizes properly
    const QImage frame = makeTextLikeFrame(QSize(640, 360));
    const QImage fullScale = ScreenEncoder::scaleImage(frame, 1.0);
    QCOMPARE(fullScale.size(), QSize(640, 360));
    const QImage halfScale = ScreenEncoder::scaleImage(frame, 0.5);
    QCOMPARE(halfScale.size(), QSize(320, 180));

    // Quality clamping in encodeAt guards against -1, 0, and >100
    const TimedEncodedFrame clampedNeg = ScreenEncoder::encodeAt(frame, -1);
    QVERIFY(clampedNeg.valid);
    QCOMPARE(clampedNeg.qualityUsed, 1);
    QVERIFY(!clampedNeg.jpeg.isEmpty());

    const TimedEncodedFrame clampedZero = ScreenEncoder::encodeAt(frame, 0);
    QVERIFY(clampedZero.valid);
    QCOMPARE(clampedZero.qualityUsed, 1);

    const TimedEncodedFrame clampedHigh = ScreenEncoder::encodeAt(frame, 150);
    QVERIFY(clampedHigh.valid);
    QCOMPARE(clampedHigh.qualityUsed, 100);
    QVERIFY(clampedHigh.jpeg.size() > clampedZero.jpeg.size());

    const TimedEncodedFrame scaledEncoded = ScreenEncoder::encodeScaledAt(frame, 0.75, 75);
    QVERIFY(scaledEncoded.valid);
    QCOMPARE(scaledEncoded.size, QSize(480, 270));
    QCOMPARE(scaledEncoded.qualityUsed, 75);
    QCOMPARE(QImage::fromData(scaledEncoded.jpeg, "JPEG").size(), QSize(480, 270));

    // Legacy compatibility checks
    const QByteArray legacyJpeg = ScreenEncoder::encodeJpeg(frame);
    QVERIFY(!legacyJpeg.isEmpty());
    const EncodedScreenFrame bounded = ScreenEncoder::encodeWithinLimit(frame, 64 * 1024);
    QVERIFY(!bounded.jpeg.isEmpty());
    QVERIFY(bounded.jpeg.size() <= 64 * 1024);
}

void AdaptiveOptimizerControllerTest::streamOptimizerEnforcesEffectivePayloadLimitAndPacketizes()
{
    StreamOptimizer optimizer;
    QCOMPARE(optimizer.effectiveMaxPayload(), qsizetype(262104));
    QCOMPARE(optimizer.effectiveMaxWireBytes(), qsizetype(262144));

    // Even if maxWireFrameBytes is configured above 256 KiB, Relay 1-packet cap holds
    StreamOptimizer::Config hugeCfg;
    hugeCfg.maxWireFrameBytes = 1024 * 1024;
    StreamOptimizer clampedOpt(hugeCfg);
    QCOMPARE(clampedOpt.effectiveMaxPayload(), qsizetype(262104));
    QCOMPARE(clampedOpt.effectiveMaxWireBytes(), qsizetype(262144));

    const QImage noisy1080p = makeNoisyFrame(QSize(1920, 1080), 99U);
    StreamOptimizer::OptimizerContext ctx;
    ctx.availableBandwidthBps = 10000000.0; // 10 MB/s: protocol limit is the binding cap
    ctx.estimatedBandwidthBps = 12500000.0;
    ctx.smoothedRttMs = 5.0;
    ctx.minRttMs = 5.0;

    const StreamOptimizer::OptimizeResult res = optimizer.optimizeFull(1, noisy1080p, ctx);
    QVERIFY(res.feasible);
    QVERIFY(!res.shouldDropFrame);
    QVERIFY(res.encoded.valid);
    QVERIFY(res.encoded.jpeg.size() <= optimizer.effectiveMaxPayload());
    QVERIFY(res.encoded.wireBytes <= optimizer.effectiveMaxWireBytes());

    const QList<QByteArray> packets = ScreenFramePacketizer::packetize(res.encoded.jpeg, 1U, 42ULL);
    QCOMPARE(packets.size(), 1);
    QVERIFY(packets.first().size() <= 262144);
}

void AdaptiveOptimizerControllerTest::streamOptimizerBinarySearchHandlesNonMonotonicJpegAndRetainsBestBuffer()
{
    StreamOptimizer::Config cfg;
    cfg.scaleRatios = {1.0};
    cfg.fpsLevels = {5};
    cfg.minQuality = 30;
    cfg.maxQuality = 90;

    StreamOptimizer optimizer(cfg);
    // Non-monotonic curve: q=74 spikes to 18000 (> budget ~15000), while q=70 is 12000 (<= budget)
    optimizer.setEncodeFunction([](const QImage &img, int quality) -> TimedEncodedFrame {
        qsizetype size = 4000 + quality * 110;
        if (quality == 74)
        {
            size = 18000;
        }
        else if (quality == 70)
        {
            size = 12000;
        }
        else if (quality > 74)
        {
            size = 17000 + (quality - 74) * 200;
        }
        return makeSyntheticTimedFrame(img.size(), quality, size, 1);
    });

    StreamOptimizer::OptimizerContext ctx;
    ctx.availableBandwidthBps = 75000.0; // 75000 / 5 = 15000 B wire budget => 14960 B JPEG budget
    ctx.estimatedBandwidthBps = 93750.0;
    ctx.minFps = 5;
    ctx.maxFps = 5;

    const QImage frame = makeTextLikeFrame(QSize(640, 360));
    const StreamOptimizer::OptimizeResult res = optimizer.optimizeFull(1, frame, ctx);
    QVERIFY(res.feasible);
    QVERIFY(!res.shouldDropFrame);
    QVERIFY(res.encoded.valid);
    QVERIFY(res.encoded.wireBytes <= 15000);
    QVERIFY(res.bestConfig.quality >= 70);
    QCOMPARE(res.encoded.qualityUsed, res.bestConfig.quality);
    QCOMPARE(res.encoded.jpeg.size(), static_cast<qsizetype>(res.encoded.wireBytes - 40));

    // Also test non-monotonic where high probe fails AFTER a valid probe succeeded
    StreamOptimizer optimizer2(cfg);
    optimizer2.setEncodeFunction([](const QImage &img, int quality) -> TimedEncodedFrame {
        qsizetype size = (quality <= 72) ? (6000 + quality * 80) : 25000;
        if (quality == 60)
        {
            size = 10000; // mid probe fits
        }
        if (quality == 75)
        {
            size = 25000; // over budget
        }
        return makeSyntheticTimedFrame(img.size(), quality, size, 1);
    });
    const StreamOptimizer::OptimizeResult res2 = optimizer2.optimizeFull(2, frame, ctx);
    QVERIFY(res2.feasible);
    QVERIFY(res2.encoded.wireBytes <= 15000);
    QVERIFY(res2.bestConfig.quality >= 60 && res2.bestConfig.quality <= 72);
}

void AdaptiveOptimizerControllerTest::streamOptimizerCachesResizesAndComputesFpsAnalytically()
{
    auto runWithFpsCount = [](const QVector<int> &fpsLevels) -> StreamOptimizer::OptimizeResult {
        StreamOptimizer::Config cfg;
        cfg.scaleRatios = {1.0, 0.85, 0.75, 0.67, 0.5};
        cfg.fpsLevels = fpsLevels;
        StreamOptimizer opt(cfg);

        opt.setEncodeFunction([](const QImage &img, int quality) -> TimedEncodedFrame {
            const double scaleArea =
                static_cast<double>(img.width() * img.height()) / (1280.0 * 720.0);
            const qsizetype bytes = static_cast<qsizetype>(
                std::lround((4000.0 + quality * 350.0) * scaleArea));
            return makeSyntheticTimedFrame(img.size(), quality, bytes, 1);
        });

        StreamOptimizer::OptimizerContext ctx;
        ctx.availableBandwidthBps = 120000.0;
        ctx.estimatedBandwidthBps = 150000.0;
        ctx.minFps = fpsLevels.first();
        ctx.maxFps = fpsLevels.last();

        const QImage frame = makeTextLikeFrame(QSize(1280, 720));
        return opt.optimizeFull(10, frame, ctx);
    };

    const StreamOptimizer::OptimizeResult res6 = runWithFpsCount({2, 3, 5, 8, 10, 15});
    const StreamOptimizer::OptimizeResult res18 = runWithFpsCount(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18});

    QVERIFY(res6.feasible);
    QVERIFY(res18.feasible);
    QVERIFY(res6.resizeCalls <= 5);
    QVERIFY(res18.resizeCalls <= 5);
    // Encode calls must scale with scales * log2(Q) (~30-45), NOT scales * fpsLevels * log2(Q) (18*5*6=540)
    QVERIFY2(res6.totalEncodeCalls <= 45,
             qPrintable(QStringLiteral("Expected <=45 encodes, got %1").arg(res6.totalEncodeCalls)));
    QVERIFY2(res18.totalEncodeCalls <= 60,
             qPrintable(QStringLiteral("Expected <=60 encodes even with 18 FPS levels, got %1")
                            .arg(res18.totalEncodeCalls)));

    // Cache hit on same frameId + unchanged QImage, and cache miss on new frameId or mutated QImage
    StreamOptimizer opt;
    int rawEncodeCalls = 0;
    opt.setEncodeFunction([&rawEncodeCalls](const QImage &img, int quality) -> TimedEncodedFrame {
        ++rawEncodeCalls;
        return makeSyntheticTimedFrame(img.size(), quality, 5000 + quality * 100, 1);
    });

    StreamOptimizer::OptimizerContext ctx;
    ctx.availableBandwidthBps = 100000.0;
    QImage frameA = makeTextLikeFrame(QSize(640, 360));

    const auto first = opt.optimizeFull(100, frameA, ctx);
    QVERIFY(first.feasible);
    QVERIFY(first.totalEncodeCalls > 0);
    const int callsAfterFirst = rawEncodeCalls;

    const auto sameFrameAgain = opt.optimizeFull(100, frameA, ctx);
    QVERIFY(sameFrameAgain.feasible);
    QCOMPARE(rawEncodeCalls, callsAfterFirst);
    QCOMPARE(sameFrameAgain.totalEncodeCalls, 0);
    QVERIFY(sameFrameAgain.encodeCacheHits > 0);

    // Different frameId must NOT hit stale frame cache
    const auto newFrameId = opt.optimizeFull(101, frameA, ctx);
    QVERIFY(newFrameId.feasible);
    QVERIFY(rawEncodeCalls > callsAfterFirst);
    const int callsAfterSecond = rawEncodeCalls;

    // Mutated QImage pixels with same frameId must detect cacheKey change and re-encode
    frameA.fill(Qt::black);
    const auto mutatedSameId = opt.optimizeFull(101, frameA, ctx);
    QVERIFY(mutatedSameId.feasible);
    QVERIFY(rawEncodeCalls > callsAfterSecond);
}

void AdaptiveOptimizerControllerTest::controllerFastLoopReusesConfigWithSingleEncodeWhenSteady()
{
    AdaptiveStreamController controller;
    int encodeCounter = 0;
    controller.setEncodeFunction([&encodeCounter](const QImage &img, int quality) -> TimedEncodedFrame {
        ++encodeCounter;
        return makeSyntheticTimedFrame(img.size(), quality, 4000 + quality * 80, 1);
    });

    const QImage frame = makeTextLikeFrame(QSize(1280, 720));
    const auto init = controller.processFrame(makeBaseInput(1, frame, 0, 200000.0, 0.02));
    QVERIFY(init.shouldSend);
    QVERIFY(init.usedSlowLoop);
    QVERIFY(init.configChanged);
    QCOMPARE(init.configChangeReason, QStringLiteral("init"));

    for (int i = 2; i <= 5; ++i)
    {
        const int before = encodeCounter;
        const auto decision = controller.processFrame(
            makeBaseInput(static_cast<quint64>(i), frame, (i - 1) * 100, 200000.0, 0.02));
        QVERIFY(decision.shouldSend);
        QVERIFY(!decision.dropped);
        QVERIFY(!decision.usedSlowLoop);
        QVERIFY(!decision.configChanged);
        QCOMPARE(decision.encodeRetryCount, 0);
        QCOMPARE(encodeCounter - before, 1);
        QVERIFY(decision.wireBytes <= decision.frameByteBudget);
    }
}

void AdaptiveOptimizerControllerTest::controllerFastLoopBoundedRetriesAndSafeDropOnImpossibleBudget()
{
    AdaptiveStreamController::Config cfg;
    cfg.maxFastRetries = 3;
    AdaptiveStreamController controller(cfg);

    bool complexSpike = false;
    bool impossible = false;
    controller.setEncodeFunction([&](const QImage &img, int quality) -> TimedEncodedFrame {
        if (impossible)
        {
            return makeSyntheticTimedFrame(img.size(), quality, 50000, 1);
        }
        if (complexSpike)
        {
            // Over budget (~25 KB) at high quality, fits at q <= 55
            const qsizetype sz = (quality > 55) ? 35000 : (6000 + quality * 150);
            return makeSyntheticTimedFrame(img.size(), quality, sz, 1);
        }
        return makeSyntheticTimedFrame(img.size(), quality, 3000 + quality * 80, 1);
    });

    const QImage frame = makeTextLikeFrame(QSize(1280, 720));
    const auto init = controller.processFrame(makeBaseInput(1, frame, 0, 120000.0, 0.1));
    QVERIFY(init.shouldSend);

    // Part A: complexity spike hits bounded Fast Loop retries and succeeds under budget
    complexSpike = true;
    const auto spikeDecision = controller.processFrame(makeBaseInput(2, frame, 100, 120000.0, 0.1));
    QVERIFY(spikeDecision.shouldSend);
    QVERIFY(!spikeDecision.dropped);
    QVERIFY(!spikeDecision.usedSlowLoop);
    QVERIFY(spikeDecision.encodeRetryCount >= 1);
    QVERIFY(spikeDecision.encodeRetryCount <= cfg.maxFastRetries);
    QVERIFY(spikeDecision.wireBytes <= spikeDecision.frameByteBudget);

    // Part B: impossible budget drops cleanly and never emits over-budget JPEG
    impossible = true;
    const auto dropDecision = controller.processFrame(makeBaseInput(3, frame, 200, 2000.0, 0.1));
    QVERIFY(!dropDecision.shouldSend);
    QVERIFY(dropDecision.dropped);
    QVERIFY(dropDecision.jpeg.isEmpty());
    QCOMPARE(dropDecision.wireBytes, qsizetype(0));
    QCOMPARE(dropDecision.configChangeReason, QStringLiteral("frame_drop"));
    QCOMPARE(controller.state().droppedFrames, qint64(1));
}

void AdaptiveOptimizerControllerTest::controllerSlowLoopTriggersOnPeriodicAndAllEvents()
{
    const QImage frame = makeTextLikeFrame(QSize(640, 360));
    auto makeFastMock = [](const QImage &img, int quality) -> TimedEncodedFrame {
        return makeSyntheticTimedFrame(img.size(), quality, 3000 + quality * 80, 2);
    };

    // 1. Periodic
    {
        AdaptiveStreamController c;
        c.setEncodeFunction(makeFastMock);
        QCOMPARE(c.processFrame(makeBaseInput(1, frame, 0, 200000.0, 0.05)).configChangeReason, QStringLiteral("init"));
        QVERIFY(!c.processFrame(makeBaseInput(2, frame, 500, 200000.0, 0.05)).usedSlowLoop);
        QVERIFY(c.processFrame(makeBaseInput(3, frame, 1000, 200000.0, 0.05)).usedSlowLoop);
    }

    // 2. Bandwidth drop
    {
        AdaptiveStreamController c;
        c.setEncodeFunction(makeFastMock);
        c.processFrame(makeBaseInput(1, frame, 0, 300000.0, 0.05));
        const auto d = c.processFrame(makeBaseInput(2, frame, 250, 40000.0, 0.05));
        QVERIFY(d.usedSlowLoop);
        QCOMPARE(d.configChangeReason, QStringLiteral("bw_drop"));
    }

    // 3. Motion up
    {
        AdaptiveStreamController c;
        c.setEncodeFunction(makeFastMock);
        c.processFrame(makeBaseInput(1, frame, 0, 150000.0, 0.02));
        const auto d = c.processFrame(makeBaseInput(2, frame, 250, 150000.0, 0.85));
        QVERIFY(d.usedSlowLoop);
        QVERIFY(d.configChanged);
        QCOMPARE(d.configChangeReason, QStringLiteral("motion_up"));
    }

    // 4. Fast Loop repeated misses
    {
        AdaptiveStreamController::Config cfg;
        cfg.fastMissTriggerCount = 2;
        AdaptiveStreamController c(cfg);
        int mode = 0;
        c.setEncodeFunction([&mode](const QImage &img, int quality) -> TimedEncodedFrame {
            if (mode == 1)
            {
                return makeSyntheticTimedFrame(img.size(), quality, 100000, 2);
            }
            return makeSyntheticTimedFrame(img.size(), quality, 3000 + quality * 80, 2);
        });
        c.processFrame(makeBaseInput(1, frame, 0, 200000.0, 0.1));
        mode = 1;
        c.processFrame(makeBaseInput(2, frame, 100, 200000.0, 0.1)); // miss 1
        c.processFrame(makeBaseInput(3, frame, 180, 200000.0, 0.1)); // miss 2
        mode = 0;
        const auto d = c.processFrame(makeBaseInput(4, frame, 250, 200000.0, 0.1));
        QVERIFY(d.usedSlowLoop);
    }

    // 5. CPU overload via cpuUsagePercent AND via encodeTimeMs fraction
    {
        AdaptiveStreamController c;
        c.setEncodeFunction(makeFastMock);
        c.processFrame(makeBaseInput(1, frame, 0, 300000.0, 0.5));
        auto highCpu = makeBaseInput(2, frame, 250, 300000.0, 0.5);
        highCpu.cpuUsagePercent = 92.0;
        const auto dCpu = c.processFrame(highCpu);
        QVERIFY(dCpu.usedSlowLoop);
        QVERIFY(dCpu.activeConfig.fps < 15);
    }
    {
        AdaptiveStreamController c;
        qint64 fakeEncodeMs = 2;
        c.setEncodeFunction([&fakeEncodeMs](const QImage &img, int quality) -> TimedEncodedFrame {
            return makeSyntheticTimedFrame(img.size(), quality, 3000 + quality * 60, fakeEncodeMs);
        });
        const auto init = c.processFrame(makeBaseInput(1, frame, 0, 400000.0, 0.8));
        QCOMPARE(init.activeConfig.fps, 15); // 66.7 ms interval, > 33.3 ms triggers encode slow
        fakeEncodeMs = 50;
        c.processFrame(makeBaseInput(2, frame, 80, 400000.0, 0.8));  // ema: 14.0
        c.processFrame(makeBaseInput(3, frame, 120, 400000.0, 0.8)); // ema: 23.0
        c.processFrame(makeBaseInput(4, frame, 160, 400000.0, 0.8)); // ema: 29.75
        c.processFrame(makeBaseInput(5, frame, 190, 400000.0, 0.8)); // ema: 34.81 (> 33.33)
        const auto dSlowEnc = c.processFrame(makeBaseInput(6, frame, 250, 400000.0, 0.8));
        QVERIFY(dSlowEnc.usedSlowLoop);
        QCOMPARE(dSlowEnc.configChangeReason, QStringLiteral("cpu_overload"));
    }

    // 6. RTT spike
    {
        AdaptiveStreamController c;
        c.setEncodeFunction(makeFastMock);
        c.processFrame(makeBaseInput(1, frame, 0, 200000.0, 0.5));
        auto rttIn = makeBaseInput(2, frame, 250, 200000.0, 0.5);
        rttIn.smoothedRttMs = 180.0;
        rttIn.minRttMs = 20.0;
        const auto dRtt = c.processFrame(rttIn);
        QVERIFY(dRtt.usedSlowLoop);
    }
}

void AdaptiveOptimizerControllerTest::controllerHysteresisFastDowngradeVsSlowUpgrade()
{
    AdaptiveStreamController::Config cfg;
    cfg.slowLoopIntervalMs = 1000;
    cfg.upgradeStabilityTicks = 3;
    cfg.upgradeStabilityWindowMs = 3000;
    cfg.resolutionCooldownMs = 2000;
    AdaptiveStreamController controller(cfg);

    controller.setEncodeFunction([](const QImage &img, int quality) -> TimedEncodedFrame {
        const double area = static_cast<double>(img.width() * img.height()) / (1280.0 * 720.0);
        const qsizetype bytes = static_cast<qsizetype>(std::lround((4000.0 + quality * 200.0) * area));
        return makeSyntheticTimedFrame(img.size(), quality, bytes, 1);
    });

    const QImage frame = makeTextLikeFrame(QSize(1280, 720));
    const auto init = controller.processFrame(makeBaseInput(1, frame, 0, 350000.0, 0.2));
    QVERIFY(init.shouldSend);
    const StreamConfig highCfg = init.activeConfig;

    // Bandwidth drop => fast downgrade in 1 tick
    const auto drop = controller.processFrame(makeBaseInput(2, frame, 500, 30000.0, 0.2));
    QVERIFY(drop.shouldSend);
    QVERIFY(drop.usedSlowLoop);
    QVERIFY(drop.configChanged);
    QCOMPARE(drop.configChangeReason, QStringLiteral("bw_drop"));
    QVERIFY(drop.activeConfig.objectiveScore < highCfg.objectiveScore);
    const StreamConfig lowCfg = drop.activeConfig;

    // Bandwidth recovers => must hold for stability window before upgrading
    const auto rec1 = controller.processFrame(makeBaseInput(3, frame, 1500, 350000.0, 0.2));
    QVERIFY(rec1.shouldSend);
    QVERIFY(!rec1.configChanged);
    QCOMPARE(rec1.activeConfig.fps, lowCfg.fps);
    QCOMPARE(rec1.activeConfig.scale, lowCfg.scale);

    const auto rec2 = controller.processFrame(makeBaseInput(4, frame, 2500, 350000.0, 0.2));
    QVERIFY(!rec2.configChanged);

    const auto rec3 = controller.processFrame(makeBaseInput(5, frame, 3500, 350000.0, 0.2));
    QVERIFY(!rec3.configChanged);

    // At t=4500 (elapsed 4500 - 1500 = 3000 ms >= 3000 ms, ticks = 4 >= 3), upgrade commits
    const auto rec4 = controller.processFrame(makeBaseInput(6, frame, 4500, 350000.0, 0.2));
    QVERIFY(rec4.shouldSend);
    QVERIFY(rec4.configChanged);
    QVERIFY(rec4.activeConfig.fps > lowCfg.fps || rec4.activeConfig.scale > lowCfg.scale ||
            rec4.activeConfig.quality > lowCfg.quality);
}

void AdaptiveOptimizerControllerTest::controllerResolutionCooldownAndEmergencyBypass()
{
    AdaptiveStreamController::Config cfg;
    cfg.resolutionCooldownMs = 2500;
    cfg.slowLoopIntervalMs = 500;
    AdaptiveStreamController controller(cfg);

    controller.setEncodeFunction([](const QImage &img, int quality) -> TimedEncodedFrame {
        const double area = static_cast<double>(img.width() * img.height()) / (1280.0 * 720.0);
        const qsizetype bytes = static_cast<qsizetype>(std::lround((14000.0 + quality * 200.0) * area));
        return makeSyntheticTimedFrame(img.size(), quality, bytes, 1);
    });

    const QImage frame = makeTextLikeFrame(QSize(1280, 720));
    const auto init = controller.processFrame(makeBaseInput(1, frame, 0, 250000.0, 0.05));
    QCOMPARE(init.activeConfig.scale, 1.0);

    // Non-emergency scale drop at t=1000 sets lastResolutionChangeMs = 1000 (scale drops from 1.0 to 0.75)
    // At B=32000 (budget(2)=16000): scale 1.0 minQ=30 is 20000 (>16000, infeasible -> scale must drop),
    // scale 0.75 minQ=30 is 11250 (<=16000, feasible; prefers 0.75 over 0.50 at low motion 0.05).
    const auto firstDrop = controller.processFrame(makeBaseInput(2, frame, 1000, 32000.0, 0.05));
    QVERIFY(firstDrop.usedSlowLoop);
    const double scaleAfterFirstDrop = firstDrop.activeConfig.scale;
    QVERIFY(scaleAfterFirstDrop < 1.0);
    QVERIFY(scaleAfterFirstDrop > 0.5);

    // At t=2000 (1000 ms < 2500 ms cooldown), a moderate motion jump where current scale is still feasible
    // must NOT change resolution
    const auto withinCooldown = controller.processFrame(makeBaseInput(3, frame, 2000, 32000.0, 0.90));
    QVERIFY(withinCooldown.shouldSend);
    QCOMPARE(withinCooldown.activeConfig.scale, scaleAfterFirstDrop);

    // At t=2200 (still inside cooldown), severe drop to B=14000 (budget(2)=7000) makes 0.75 infeasible
    // even at minQ/minFps (11250 > 7000), while 0.5 at minQ=30 is 5000 (<=7000):
    // emergency_drop MUST bypass cooldown and drop scale!
    const auto emergency = controller.processFrame(makeBaseInput(4, frame, 2200, 14000.0, 0.90));
    QVERIFY(emergency.shouldSend);
    QVERIFY(emergency.configChanged);
    QVERIFY(emergency.activeConfig.scale < scaleAfterFirstDrop);
    QCOMPARE(emergency.configChangeReason, QStringLiteral("emergency_drop"));
}

void AdaptiveOptimizerControllerTest::controllerMinScoreImprovementPreventsOscillation()
{
    AdaptiveStreamController::Config cfg;
    cfg.slowLoopIntervalMs = 500;
    cfg.minScoreImprovement = 0.05;
    AdaptiveStreamController controller(cfg);

    controller.setEncodeFunction([](const QImage &img, int quality) -> TimedEncodedFrame {
        const double area = static_cast<double>(img.width() * img.height()) / (1280.0 * 720.0);
        const qsizetype bytes = static_cast<qsizetype>(std::lround((3000.0 + quality * 120.0) * area));
        return makeSyntheticTimedFrame(img.size(), quality, bytes, 1);
    });

    const QImage frame = makeTextLikeFrame(QSize(1280, 720));
    const auto init = controller.processFrame(makeBaseInput(1, frame, 0, 150000.0, 0.10));
    QVERIFY(init.shouldSend);

    int switches = 0;
    for (int i = 1; i <= 20; ++i)
    {
        const double bwJitter = (i % 2 == 0) ? 1.05 : 0.95;
        const double motionJitter = (i % 2 == 0) ? 0.13 : 0.07;
        const auto d = controller.processFrame(
            makeBaseInput(static_cast<quint64>(i + 1), frame, i * 500, 150000.0 * bwJitter, motionJitter));
        QVERIFY(d.shouldSend);
        if (d.configChanged)
        {
            ++switches;
        }
    }
    QCOMPARE(switches, 0);
}

void AdaptiveOptimizerControllerTest::controllerLowMotionTextReadabilityBiasAndHighMotionFps()
{
    const QImage textFrame = makeTextLikeFrame(QSize(1280, 720));
    auto mockEncoder = [](const QImage &img, int quality) -> TimedEncodedFrame {
        const double area = static_cast<double>(img.width() * img.height()) / (1280.0 * 720.0);
        const qsizetype bytes = static_cast<qsizetype>(std::lround((4500.0 + quality * 180.0) * area));
        return makeSyntheticTimedFrame(img.size(), quality, bytes, 1);
    };

    // Low motion: MotionDetector + controller prioritize sharpness/scale >= 0.85
    MotionDetector staticDetector;
    staticDetector.analyze(textFrame);
    const MotionResult staticMotion = staticDetector.analyze(textFrame);

    AdaptiveStreamController staticController;
    staticController.setEncodeFunction(mockEncoder);
    auto staticIn = makeBaseInput(1, textFrame, 0, 120000.0, staticMotion.smoothedScore);
    staticIn.wQ = staticMotion.wQ;
    staticIn.wR = staticMotion.wR;
    staticIn.wP = staticMotion.wP;
    const auto staticDecision = staticController.processFrame(staticIn);
    QVERIFY(staticDecision.shouldSend);
    QVERIFY(staticDecision.scaleUsed >= 0.85);

    // High motion: MotionDetector + controller shift toward higher FPS (>= 8)
    AdaptiveStreamController motionController;
    motionController.setEncodeFunction(mockEncoder);
    auto motionIn = makeBaseInput(1, textFrame, 0, 120000.0, 0.90);
    const auto motionDecision = motionController.processFrame(motionIn);
    QVERIFY(motionDecision.shouldSend);
    QVERIFY(motionDecision.targetFps >= 8);
    QVERIFY(motionDecision.targetFps > staticDecision.targetFps);
}

void AdaptiveOptimizerControllerTest::controllerFloorRecoveryMitigatesSub4096Trap()
{
    // Push BandwidthEstimator to its 16,000 B/s floor on a 220 ms RTT link
    BandwidthEstimator::Config bwCfg;
    bwCfg.initialBandwidthBps = 20000.0;
    bwCfg.minBandwidthBps = 16000.0;
    BandwidthEstimator estimator(bwCfg);

    estimator.onFrameSent(1, 5000, 1, 0, 0);
    estimator.onAckTimeout(1, 1500, 0);
    estimator.onFrameAcknowledged(1, 1600, 0);
    QCOMPARE(estimator.estimatedBandwidthBps(), 16000.0);
    QCOMPARE(estimator.availableBandwidthBps(), 12800.0);

    AdaptiveStreamController controller;
    controller.setEncodeFunction([](const QImage &img, int quality) -> TimedEncodedFrame {
        const double area = static_cast<double>(img.width() * img.height()) / (1280.0 * 720.0);
        const qsizetype bytes = static_cast<qsizetype>(std::lround((2000.0 + quality * 120.0) * area));
        return makeSyntheticTimedFrame(img.size(), quality, bytes, 1);
    });

    const QImage frame = makeTextLikeFrame(QSize(1280, 720));
    auto in = makeBaseInput(1, frame, 2000, estimator.availableBandwidthBps(), 0.25);
    in.estimatedBandwidthBps = estimator.estimatedBandwidthBps();
    in.smoothedRttMs = 220.0;
    in.minRttMs = 220.0;

    const auto d = controller.processFrame(in);
    QVERIFY(d.shouldSend);
    // Floor recovery must choose minFps=2 giving >= 4096 B budget instead of 5 FPS (2560 B < 4096 B)
    QCOMPARE(d.targetFps, 2);
    QVERIFY(d.frameByteBudget >= 4096);
    QVERIFY(d.wireBytes >= 4096);

    // Simulate link recovering to ~200 KB/s (220 ms prop RTT + ~25 ms tx = 245 ms ACK)
    qint64 now = 2100;
    for (quint32 fid = 2; fid <= 6; ++fid)
    {
        estimator.onFrameSent(fid, d.wireBytes, d.encodeTimeMs, 0, now);
        now += 245;
        estimator.onFrameAcknowledged(fid, now, 0);
    }
    QVERIFY2(estimator.estimatedBandwidthBps() > 16000.0,
             qPrintable(QStringLiteral("Bandwidth remained stuck at floor: %1")
                            .arg(estimator.estimatedBandwidthBps())));
}

void AdaptiveOptimizerControllerTest::controllerMinBoundsHighRttSurvivalAndSafeFallback()
{
    AdaptiveStreamController::Config cfg;
    cfg.minQuality = 40;
    cfg.minScale = 0.67;
    cfg.minFps = 3;
    AdaptiveStreamController controller(cfg);

    controller.setEncodeFunction([](const QImage &img, int quality) -> TimedEncodedFrame {
        const double area = static_cast<double>(img.width() * img.height()) / (1280.0 * 720.0);
        const qsizetype bytes = static_cast<qsizetype>(std::lround((2500.0 + quality * 100.0) * area));
        return makeSyntheticTimedFrame(img.size(), quality, bytes, 1);
    });

    const QImage frame = makeTextLikeFrame(QSize(1280, 720));
    auto highRttIn = makeBaseInput(1, frame, 0, 100000.0, 0.3);
    highRttIn.smoothedRttMs = 600.0; // > 1000 / minFps (333 ms)
    highRttIn.minRttMs = 600.0;

    const auto d = controller.processFrame(highRttIn);
    QVERIFY(d.shouldSend);
    QVERIFY(!d.dropped);
    QCOMPARE(d.targetFps, 3);
    QVERIFY(d.qualityUsed >= 40);
    QVERIFY(d.scaleUsed >= 0.67 - 1e-6);
    QVERIFY(d.wireBytes <= d.frameByteBudget);

    // Safe fallback when normal optimizer search fails all candidate FPS except minFps exact check
    AdaptiveStreamController fbController;
    fbController.setEncodeFunction([](const QImage &img, int quality) -> TimedEncodedFrame {
        // Fits only at minScale (0.5) & minQuality (30) (~4500 B), fails at larger scales/qualities
        const double area = static_cast<double>(img.width() * img.height()) / (1280.0 * 720.0);
        const qsizetype bytes = static_cast<qsizetype>(std::lround((8000.0 + quality * 200.0) * area));
        return makeSyntheticTimedFrame(img.size(), quality, bytes, 1);
    });
    const auto fbDecision = fbController.processFrame(makeBaseInput(1, frame, 0, 12000.0, 0.2));
    QVERIFY(fbDecision.shouldSend);
    QVERIFY(fbDecision.wireBytes <= 6000);
}

void AdaptiveOptimizerControllerTest::controllerNextFrameDelayAvoidsRttDoubleCounting()
{
    // 1. Low RTT: target=100ms, tx=10ms, enc=5ms, rtt=20ms, ack=true => 100 - 5 - 20 = 75 ms
    QCOMPARE(AdaptiveStreamController::computeNextFrameDelayMs(1250, 5, 20.0, 125000.0, 10, true),
             qint64(75));

    // 2. High RTT (> frameInterval): target=200ms, rtt=300ms, enc=5ms, ack=true => 0 ms (no extra wait)
    QCOMPARE(AdaptiveStreamController::computeNextFrameDelayMs(2500, 5, 300.0, 125000.0, 5, true),
             qint64(0));

    // 3. Serialization-limited: frameBytes=50000, B=100000 => tx=500ms
    QCOMPARE(AdaptiveStreamController::computeNextFrameDelayMs(50000, 5, 20.0, 100000.0, 10, true),
             qint64(0));
    QCOMPARE(AdaptiveStreamController::computeNextFrameDelayMs(50000, 5, 20.0, 100000.0, 10, false),
             qint64(495));

    // 4. Non-ACK mode: target=100ms, enc=5ms, ack=false => 95 ms
    QCOMPARE(AdaptiveStreamController::computeNextFrameDelayMs(2000, 5, 30.0, 100000.0, 10, false),
             qint64(95));
}

void AdaptiveOptimizerControllerTest::controllerPopulatesAll15MetricsFieldsWithShortTokens()
{
    AdaptiveStreamController controller;
    controller.setEncodeFunction([](const QImage &img, int quality) -> TimedEncodedFrame {
        return makeSyntheticTimedFrame(img.size(), quality, 3000 + quality * 80, 3);
    });

    StreamMetrics metrics(0);
    QStringList loggedLines;
    metrics.setLogSink([&loggedLines](const QString &line) {
        loggedLines.append(line);
    });

    const QImage frame = makeTextLikeFrame(QSize(1280, 720));
    const QRegularExpression tokenPattern(QStringLiteral("^[a-z0-9_]*$"));

    const auto d1 = controller.processFrame(makeBaseInput(1, frame, 0, 200000.0, 0.05));
    const auto d2 = controller.processFrame(makeBaseInput(2, frame, 100, 200000.0, 0.05));
    const auto d3 = controller.processFrame(makeBaseInput(3, frame, 350, 30000.0, 0.05));

    for (const auto &d : {d1, d2, d3})
    {
        QVERIFY(tokenPattern.match(d.configChangeReason).hasMatch());
        QVERIFY(d.metricsSnapshot.estimatedBandwidthBps > 0.0);
        QVERIFY(d.metricsSnapshot.frameByteBudget > 0);
        QVERIFY(d.metricsSnapshot.actualFrameBytes > 0);
        QVERIFY(d.metricsSnapshot.jpegQuality > 0);
        QVERIFY(d.metricsSnapshot.resolution.isValid());
        QVERIFY(d.metricsSnapshot.targetFps > 0);
        QVERIFY(d.metricsSnapshot.actualFps >= 0.0);
        QVERIFY(d.metricsSnapshot.encodeTimeMs >= 0);
        QVERIFY(d.metricsSnapshot.ackLatencyMs > 0.0);
        QVERIFY(metrics.maybeLog(d.metricsSnapshot, 1000));
    }

    QCOMPARE(loggedLines.size(), 3);
    for (const QString &line : loggedLines)
    {
        QVERIFY(line.contains(QStringLiteral("estBwBps=")));
        QVERIFY(line.contains(QStringLiteral("obsThroughputBps=")));
        QVERIFY(line.contains(QStringLiteral("byteBudget=")));
        QVERIFY(line.contains(QStringLiteral("frameBytes=")));
        QVERIFY(line.contains(QStringLiteral("motion=")));
        QVERIFY(line.contains(QStringLiteral("quality=")));
        QVERIFY(line.contains(QStringLiteral("res=")));
        QVERIFY(line.contains(QStringLiteral("targetFps=")));
        QVERIFY(line.contains(QStringLiteral("actualFps=")));
        QVERIFY(line.contains(QStringLiteral("encodeMs=")));
        QVERIFY(line.contains(QStringLiteral("retries=")));
        QVERIFY(line.contains(QStringLiteral("ackRttMs=")));
        QVERIFY(line.contains(QStringLiteral("pendingBytes=")));
        QVERIFY(line.contains(QStringLiteral("dropped=")));
        QVERIFY(line.contains(QStringLiteral("reason=")));
    }
}

void AdaptiveOptimizerControllerTest::controllerResetClearsStateCleanly()
{
    AdaptiveStreamController controller;
    const QImage frame = makeTextLikeFrame(QSize(640, 360));
    const auto d1 = controller.processFrame(makeBaseInput(1, frame, 0, 150000.0, 0.1));
    QVERIFY(d1.shouldSend);
    QVERIFY(controller.state().hasActiveConfig);
    QCOMPARE(controller.state().slowLoopCount, qint64(1));

    controller.reset();
    const auto st = controller.state();
    QVERIFY(!st.hasActiveConfig);
    QCOMPARE(st.slowLoopCount, qint64(0));
    QCOMPARE(st.fastLoopCount, qint64(0));
    QCOMPARE(st.droppedFrames, qint64(0));
    QCOMPARE(st.lastSlowLoopMs, qint64(-1));

    const auto d2 = controller.processFrame(makeBaseInput(2, frame, 50, 150000.0, 0.1));
    QVERIFY(d2.shouldSend);
    QVERIFY(d2.usedSlowLoop);
    QCOMPARE(d2.configChangeReason, QStringLiteral("init"));
}

void AdaptiveOptimizerControllerTest::streamOptimizerRetainsCrossFpsProbedMidpointCandidates()
{
    StreamOptimizer::Config cfg;
    cfg.scaleRatios = {1.0};
    cfg.fpsLevels = {10, 15};
    cfg.minQuality = 30;
    cfg.maxQuality = 90;
    cfg.processingMarginMs = 0.0;

    StreamOptimizer optimizer(cfg);
    optimizer.setEncodeFunction([](const QImage &scaledImage, int quality) {
        TimedEncodedFrame res;
        res.size = scaledImage.size();
        res.encodeTimeMs = 1;
        res.valid = true;
        int sz = 6700;
        if (quality > 30 && quality <= 60)
        {
            sz = 6700 + (quality - 30) * 100; // q=60 -> 9700 B
        }
        else if (quality > 60)
        {
            sz = 10000 + (quality - 61) * 150; // q=61..90 -> >= 10000 B
        }
        res.jpeg = QByteArray(sz, 'x');
        return res;
    });

    StreamOptimizer::OptimizerContext ctx;
    ctx.availableBandwidthBps = 100000.0; // 15 FPS maxJpeg=6626, 10 FPS maxJpeg=9960
    ctx.estimatedBandwidthBps = 125000.0;
    ctx.smoothedRttMs = 0.0;
    ctx.minRttMs = 0.0;
    ctx.estimatedEncodeTimeMs = 0.0;
    ctx.wQ = 0.50;
    ctx.wR = 0.30;
    ctx.wP = 0.20;
    ctx.minQuality = 30;
    ctx.maxQuality = 90;
    ctx.minScale = 1.0;
    ctx.maxScale = 1.0;
    ctx.minFps = 10;
    ctx.maxFps = 15;
    ctx.warmStartQuality = -1;

    const QImage frame(QSize(320, 180), QImage::Format_RGB32);
    const auto out = optimizer.optimizeFull(42, frame, ctx);
    QVERIFY(out.feasible);
    QCOMPARE(out.bestConfig.fps, 10);
    QCOMPARE(out.bestConfig.quality, 60);
    QCOMPARE(out.encoded.jpeg.size(), qsizetype(9700));
}

QTEST_MAIN(AdaptiveOptimizerControllerTest)
#include "AdaptiveOptimizerControllerTest.moc"
