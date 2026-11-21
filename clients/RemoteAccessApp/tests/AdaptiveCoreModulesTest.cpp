#include <QtTest>

#include "Streaming/BandwidthEstimator.h"
#include "Streaming/MotionDetector.h"
#include "Streaming/StreamMetrics.h"

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QScopeGuard>
#include <QStringList>
#include <QtGlobal>

#include <cmath>

class AdaptiveCoreModulesTest final : public QObject
{
    Q_OBJECT

private slots:
    // MotionDetector (R4)
    void motionDetectorRejectsNullAndEmptyFramesWithoutCorruptingState();
    void motionDetectorFirstFrameAndStaticNoiseFloor();
    void motionDetectorDistinguishesStaticFromHighMotionAndShiftsWeights();
    void motionDetectorSupportsMultipleFormatsAndResolutionChanges();
    void motionDetectorClampsAndNormalizesCustomWeights();

    // StreamMetrics (R8)
    void streamMetricsRateLimitsAndEmitsAll15Fields();
    void streamMetricsPreservesPendingConfigChangeReasonAcrossSuppressedTicks();
    void streamMetricsTracksActualFpsDroppedFramesAndReset();

    // BandwidthEstimator (R5)
    void bandwidthEstimatorInitialStateAndEnvConfig();
    void bandwidthEstimatorSmallFrameAntiSpiralOnHighRtt();
    void bandwidthEstimatorSteadySaturationConvergence();
    void bandwidthEstimatorGradualUpwardProbing();
    void bandwidthEstimatorFastBackoffOnQueueGrowth();
    void bandwidthEstimatorFastBackoffOnRttSpike();
    void bandwidthEstimatorFastBackoffOnAckTimeout();
    void bandwidthEstimatorRecoveryAfterCongestion();
    void bandwidthEstimatorEdgeCasesAndBoundedMap();
};

namespace
{
QImage makeSolidFrame(const QSize &size, const QColor &color, QImage::Format format = QImage::Format_RGB32)
{
    QImage img(size, format);
    img.fill(color);
    return img;
}

QImage makeAlternatingStripeFrame(const QSize &size, bool invert)
{
    QImage img(size, QImage::Format_RGB32);
    for (int y = 0; y < size.height(); ++y)
    {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        const int rowBlock = (y / 16) % 2;
        for (int x = 0; x < size.width(); ++x)
        {
            const int colBlock = (x / 16) % 2;
            const bool white = ((rowBlock ^ colBlock) == 0) ^ invert;
            line[x] = white ? qRgb(255, 255, 255) : qRgb(0, 0, 0);
        }
    }
    return img;
}
} // namespace

void AdaptiveCoreModulesTest::motionDetectorRejectsNullAndEmptyFramesWithoutCorruptingState()
{
    MotionDetector detector;

    const QImage frameA = makeSolidFrame(QSize(1280, 720), QColor(40, 80, 120));
    const MotionResult first = detector.analyze(frameA);
    QVERIFY(first.valid);
    QCOMPARE(first.rawScore, 0.0);
    QCOMPARE(first.smoothedScore, 0.0);

    const MotionResult nullRes = detector.analyze(QImage{});
    QVERIFY(!nullRes.valid);
    QCOMPARE(nullRes.smoothedScore, 0.0);

    const MotionResult emptyRes = detector.analyze(QImage(0, 0, QImage::Format_RGB32));
    QVERIFY(!emptyRes.valid);
    QCOMPARE(emptyRes.smoothedScore, 0.0);

    // Baseline frameA must remain intact after invalid inputs.
    const MotionResult repeatA = detector.analyze(frameA);
    QVERIFY(repeatA.valid);
    QCOMPARE(repeatA.rawScore, 0.0);
    QCOMPARE(repeatA.smoothedScore, 0.0);
}

void AdaptiveCoreModulesTest::motionDetectorFirstFrameAndStaticNoiseFloor()
{
    MotionDetector detector;

    const QImage base = makeSolidFrame(QSize(1920, 1080), QColor(100, 100, 100));
    const MotionResult r1 = detector.analyze(base);
    QVERIFY(r1.valid);
    QCOMPARE(r1.rawScore, 0.0);
    QCOMPARE(r1.smoothedScore, 0.0);
    QVERIFY(qFuzzyCompare(r1.wQ, 0.35));
    QVERIFY(qFuzzyCompare(r1.wR, 0.50));
    QVERIFY(qFuzzyCompare(r1.wP, 0.15));
    QVERIFY(qFuzzyCompare(r1.wQ + r1.wR + r1.wP, 1.0));

    const MotionResult r2 = detector.analyze(base);
    QVERIFY(r2.valid);
    QCOMPARE(r2.rawScore, 0.0);
    QCOMPARE(r2.smoothedScore, 0.0);

    // +4 channel jitter is below pixelDiffThreshold (8) -> treated as static noise floor.
    const QImage jittered = makeSolidFrame(QSize(1920, 1080), QColor(104, 104, 104));
    const MotionResult r3 = detector.analyze(jittered);
    QVERIFY(r3.valid);
    QCOMPARE(r3.rawScore, 0.0);
    QCOMPARE(r3.smoothedScore, 0.0);
}

void AdaptiveCoreModulesTest::motionDetectorDistinguishesStaticFromHighMotionAndShiftsWeights()
{
    MotionDetector detector;
    const QSize hd(1920, 1080);

    const MotionResult init = detector.analyze(makeAlternatingStripeFrame(hd, false));
    QVERIFY(init.valid);
    QCOMPARE(init.smoothedScore, 0.0);

    double prevSmoothed = 0.0;
    MotionResult latest;
    for (int i = 1; i <= 10; ++i)
    {
        const QImage frame = makeAlternatingStripeFrame(hd, (i % 2) == 1);
        latest = detector.analyze(frame);
        QVERIFY(latest.valid);
        QCOMPARE(latest.rawScore, 1.0);
        QVERIFY(latest.smoothedScore > prevSmoothed);
        QVERIFY(qFuzzyCompare(latest.wQ + latest.wR + latest.wP, 1.0));
        prevSmoothed = latest.smoothedScore;
    }

    // With emaAlpha = 0.30 and rawScore = 1.0 over 10 frames: 1 - 0.7^10 = 0.97175
    QVERIFY(latest.smoothedScore > 0.97);
    QVERIFY(latest.wP > 0.50);
    QVERIFY(latest.wQ < 0.22);
    QVERIFY(latest.wR < 0.27);

    // Static frames decay smoothedScore back toward 0 and shift weights back to quality/resolution.
    const QImage staticFrame = makeAlternatingStripeFrame(hd, false);
    detector.analyze(staticFrame);
    for (int i = 0; i < 15; ++i)
    {
        latest = detector.analyze(staticFrame);
        QVERIFY(latest.valid);
        QCOMPARE(latest.rawScore, 0.0);
        QVERIFY(qFuzzyCompare(latest.wQ + latest.wR + latest.wP, 1.0));
    }
    QVERIFY(latest.smoothedScore < 0.01);
    QVERIFY(latest.wQ > 0.34);
    QVERIFY(latest.wR > 0.49);
    QVERIFY(latest.wP < 0.16);
}

void AdaptiveCoreModulesTest::motionDetectorSupportsMultipleFormatsAndResolutionChanges()
{
    MotionDetector detector;
    const QColor solid(64, 128, 192);

    const MotionResult f1 = detector.analyze(makeSolidFrame(QSize(1920, 1080), solid, QImage::Format_RGB32));
    QVERIFY(f1.valid);

    const MotionResult f2 = detector.analyze(makeSolidFrame(QSize(1920, 1080), solid, QImage::Format_ARGB32_Premultiplied));
    QVERIFY(f2.valid);
    QCOMPARE(f2.rawScore, 0.0);

    const MotionResult f3 = detector.analyze(makeSolidFrame(QSize(1280, 720), solid, QImage::Format_RGB888));
    QVERIFY(f3.valid);
    QCOMPARE(f3.rawScore, 0.0);

    QImage gray(QSize(800, 600), QImage::Format_Grayscale8);
    gray.fill(static_cast<uchar>(qGray(solid.rgb())));
    const MotionResult f4 = detector.analyze(gray);
    QVERIFY(f4.valid);
    QVERIFY(f4.rawScore >= 0.0 && f4.rawScore <= 1.0);
    QCOMPARE(f4.rawScore, 0.0);

    detector.reset();
    QCOMPARE(detector.smoothedScore(), 0.0);
    const MotionResult afterReset = detector.analyze(makeAlternatingStripeFrame(QSize(1920, 1080), true));
    QVERIFY(afterReset.valid);
    QCOMPARE(afterReset.rawScore, 0.0);
    QCOMPARE(afterReset.smoothedScore, 0.0);
}

void AdaptiveCoreModulesTest::motionDetectorClampsAndNormalizesCustomWeights()
{
    MotionDetector::Config cfg;
    cfg.wQBase = 0.20;
    cfg.wQSlope = 0.50; // Would become -0.30 at motionScore = 1.0 without clamping
    cfg.wRBase = 0.30;
    cfg.wRSlope = 0.40; // Would become -0.10 at motionScore = 1.0 without clamping
    cfg.wPBase = 0.15;
    cfg.wPSlope = 0.40;
    cfg.emaAlpha = 1.0;

    MotionDetector detector(cfg);
    detector.analyze(makeSolidFrame(QSize(1920, 1080), Qt::black));
    const MotionResult res = detector.analyze(makeSolidFrame(QSize(1920, 1080), Qt::white));

    QVERIFY(res.valid);
    QCOMPARE(res.rawScore, 1.0);
    QCOMPARE(res.smoothedScore, 1.0);
    QVERIFY(res.wQ >= 0.0);
    QVERIFY(res.wR >= 0.0);
    QVERIFY(res.wP >= 0.0);
    QVERIFY(qFuzzyCompare(res.wQ + res.wR + res.wP, 1.0));
}

void AdaptiveCoreModulesTest::streamMetricsRateLimitsAndEmitsAll15Fields()
{
    StreamMetrics metrics(1000);
    QStringList emitted;
    metrics.setLogSink([&emitted](const QString &line) {
        emitted.append(line);
    });

    StreamMetricsSnapshot snap;
    snap.estimatedBandwidthBps = 500000.0;
    snap.observedThroughputBps = 420000.5;
    snap.frameByteBudget = 62500;
    snap.actualFrameBytes = 48123;
    snap.motionScore = 0.250;
    snap.jpegQuality = 78;
    snap.resolution = QSize(1280, 720);
    snap.targetFps = 8;
    snap.actualFps = 7.95;
    snap.encodeTimeMs = 9;
    snap.encodeRetryCount = 2;
    snap.ackLatencyMs = 42.5;
    snap.pendingBytes = 1024;
    snap.droppedFrames = 3;
    snap.configChangeReason = QStringLiteral("bw_drop");

    QVERIFY(metrics.maybeLog(snap, 0));
    QCOMPARE(emitted.size(), 1);
    QCOMPARE(metrics.logCount(), qint64(1));

    QVERIFY(!metrics.maybeLog(snap, 200));
    QVERIFY(!metrics.maybeLog(snap, 999));
    QCOMPARE(emitted.size(), 1);

    QVERIFY(metrics.maybeLog(snap, 1000));
    QCOMPARE(emitted.size(), 2);
    QCOMPARE(metrics.logCount(), qint64(2));

    const QString &line = emitted.last();
    QVERIFY(line.startsWith(QStringLiteral("[StreamMetrics]")));
    QVERIFY(line.contains(QStringLiteral("estBwBps=500000.0")));
    QVERIFY(line.contains(QStringLiteral("obsThroughputBps=420000.5")));
    QVERIFY(line.contains(QStringLiteral("byteBudget=62500")));
    QVERIFY(line.contains(QStringLiteral("frameBytes=48123")));
    QVERIFY(line.contains(QStringLiteral("motion=0.250")));
    QVERIFY(line.contains(QStringLiteral("quality=78")));
    QVERIFY(line.contains(QStringLiteral("res=1280x720")));
    QVERIFY(line.contains(QStringLiteral("targetFps=8")));
    QVERIFY(line.contains(QStringLiteral("actualFps=7.95")));
    QVERIFY(line.contains(QStringLiteral("encodeMs=9")));
    QVERIFY(line.contains(QStringLiteral("retries=2")));
    QVERIFY(line.contains(QStringLiteral("ackRttMs=42.5")));
    QVERIFY(line.contains(QStringLiteral("pendingBytes=1024")));
    QVERIFY(line.contains(QStringLiteral("dropped=3")));
    QVERIFY(line.contains(QStringLiteral("reason=\"bw_drop\"")));
}

void AdaptiveCoreModulesTest::streamMetricsPreservesPendingConfigChangeReasonAcrossSuppressedTicks()
{
    StreamMetrics metrics(1000);
    QStringList emitted;
    metrics.setLogSink([&emitted](const QString &line) {
        emitted.append(line);
    });

    StreamMetricsSnapshot steadySnap;
    steadySnap.resolution = QSize(1920, 1080);

    QVERIFY(metrics.maybeLog(steadySnap, 0));
    QVERIFY(emitted.last().contains(QStringLiteral("reason=\"steady\"")));

    StreamMetricsSnapshot spikeSnap = steadySnap;
    spikeSnap.configChangeReason = QStringLiteral("rtt_spike");
    QVERIFY(!metrics.maybeLog(spikeSnap, 300));

    QVERIFY(metrics.maybeLog(steadySnap, 1000));
    QVERIFY(emitted.last().contains(QStringLiteral("reason=\"rtt_spike\"")));

    QVERIFY(metrics.maybeLog(steadySnap, 2000));
    QVERIFY(emitted.last().contains(QStringLiteral("reason=\"steady\"")));
}

void AdaptiveCoreModulesTest::streamMetricsTracksActualFpsDroppedFramesAndReset()
{
    StreamMetrics metrics(1000);
    for (int i = 0; i <= 10; ++i)
    {
        metrics.recordFrameSent(i * 100);
    }
    QVERIFY(qAbs(metrics.actualFps(1000) - 10.0) < 0.05);

    metrics.recordDroppedFrame();
    metrics.recordDroppedFrame();
    metrics.recordDroppedFrame();
    QCOMPARE(metrics.droppedFrames(), qint64(3));

    QStringList emitted;
    metrics.setLogSink([&emitted](const QString &line) {
        emitted.append(line);
    });
    QVERIFY(metrics.maybeLog(StreamMetricsSnapshot{}, 1000));
    QCOMPARE(metrics.logCount(), qint64(1));

    metrics.reset();
    QCOMPARE(metrics.droppedFrames(), qint64(0));
    QCOMPARE(metrics.actualFps(0), 0.0);
    QCOMPARE(metrics.logCount(), qint64(0));
    QVERIFY(metrics.lastFormattedLine().isEmpty());
    QVERIFY(metrics.maybeLog(StreamMetricsSnapshot{}, 50));
    QCOMPARE(metrics.logCount(), qint64(1));
}

void AdaptiveCoreModulesTest::bandwidthEstimatorInitialStateAndEnvConfig()
{
    const BandwidthEstimator defaultEst;
    QCOMPARE(defaultEst.estimatedBandwidthBps(), 125000.0);
    QCOMPARE(defaultEst.availableBandwidthBps(), 100000.0);
    QCOMPARE(defaultEst.observedThroughputBps(), 0.0);
    QCOMPARE(defaultEst.smoothedRttMs(), 0.0);
    QCOMPARE(defaultEst.minRttMs(), 0.0);
    QVERIFY(!defaultEst.isCongested());
    QCOMPARE(defaultEst.inFlightCount(), 0);

    BandwidthEstimator::Config customCfg;
    customCfg.initialBandwidthBps = 400000.0;
    customCfg.safetyFactor = 0.75;
    const BandwidthEstimator customEst(customCfg);
    QCOMPARE(customEst.estimatedBandwidthBps(), 400000.0);
    QCOMPARE(customEst.availableBandwidthBps(), 300000.0);

    const bool hadEnv = qEnvironmentVariableIsSet("REMOTE_STREAM_KBPS");
    const QByteArray oldEnv = qgetenv("REMOTE_STREAM_KBPS");
    const auto restoreEnv = qScopeGuard([hadEnv, oldEnv]() {
        if (hadEnv)
        {
            qputenv("REMOTE_STREAM_KBPS", oldEnv);
        }
        else
        {
            qunsetenv("REMOTE_STREAM_KBPS");
        }
    });

    qunsetenv("REMOTE_STREAM_KBPS");
    QCOMPARE(BandwidthEstimator::Config::fromEnvironment().initialBandwidthBps, 125000.0);

    qputenv("REMOTE_STREAM_KBPS", "4000");
    QCOMPARE(BandwidthEstimator::Config::fromEnvironment().initialBandwidthBps, 500000.0);

    qputenv("REMOTE_STREAM_KBPS", "64");
    QCOMPARE(BandwidthEstimator::Config::fromEnvironment().initialBandwidthBps, 16000.0);

    qputenv("REMOTE_STREAM_KBPS", "99999");
    QCOMPARE(BandwidthEstimator::Config::fromEnvironment().initialBandwidthBps, 1500000.0);

    qputenv("REMOTE_STREAM_KBPS", "bad");
    QCOMPARE(BandwidthEstimator::Config::fromEnvironment().initialBandwidthBps, 125000.0);
}

void AdaptiveCoreModulesTest::bandwidthEstimatorSmallFrameAntiSpiralOnHighRtt()
{
    BandwidthEstimator::Config cfg;
    cfg.initialBandwidthBps = 1000000.0;
    cfg.minBandwidthBps = 16000.0;
    BandwidthEstimator est(cfg);

    qint64 nowMs = 1000;
    for (quint32 i = 1; i <= 30; ++i)
    {
        const qsizetype bytes = 8000 + static_cast<qsizetype>((i % 7) * 1000); // [8000, 14000] < 24576
        const qint64 rttMs = 95 + static_cast<qint64>(i % 11);               // [95, 105] ms
        est.onFrameSent(i, bytes, 4, bytes, nowMs);
        est.onFrameAcknowledged(i, nowMs + rttMs, 0);
        nowMs += 200;
    }

    QVERIFY(est.estimatedBandwidthBps() >= 950000.0);
    QVERIFY(est.observedThroughputBps() >= 75000.0 && est.observedThroughputBps() <= 160000.0);
    QVERIFY(est.minRttMs() >= 95.0 && est.minRttMs() <= 105.0);
    QVERIFY(!est.isCongested());
}

void AdaptiveCoreModulesTest::bandwidthEstimatorSteadySaturationConvergence()
{
    // (a) Upward convergence from 125 KB/s toward ~4 Mbps (500 KB/s) + 30 ms RTT
    {
        BandwidthEstimator::Config cfg;
        cfg.initialBandwidthBps = 125000.0;
        BandwidthEstimator est(cfg);

        est.onFrameSent(1, 4000, 2, 4000, 0);
        est.onFrameAcknowledged(1, 31, 0);

        qint64 t = 100;
        for (quint32 i = 2; i <= 21; ++i)
        {
            est.onFrameSent(i, 100000, 5, 100000, t);
            est.onFrameAcknowledged(i, t + 230, 0);
            t += 250;
        }

        QVERIFY2(est.estimatedBandwidthBps() >= 420000.0 && est.estimatedBandwidthBps() <= 620000.0,
                 qPrintable(QStringLiteral("est=%1").arg(est.estimatedBandwidthBps())));
        QVERIFY(!est.isCongested());
    }

    // (b) Downward convergence from 2 MB/s toward ~2 Mbps (250 KB/s) + 40 ms RTT
    {
        BandwidthEstimator::Config cfg;
        cfg.initialBandwidthBps = 2000000.0;
        BandwidthEstimator est(cfg);

        qint64 t = 100;
        for (quint32 i = 1; i <= 20; ++i)
        {
            est.onFrameSent(i, 100000, 5, 100000, t);
            est.onFrameAcknowledged(i, t + 440, 0);
            t += 460;
        }

        QVERIFY2(est.estimatedBandwidthBps() >= 190000.0 && est.estimatedBandwidthBps() <= 280000.0,
                 qPrintable(QStringLiteral("est=%1").arg(est.estimatedBandwidthBps())));
        QVERIFY(est.estimatedBandwidthBps() > cfg.minBandwidthBps);
        QVERIFY(!est.isCongested());
    }
}

void AdaptiveCoreModulesTest::bandwidthEstimatorGradualUpwardProbing()
{
    BandwidthEstimator::Config cfg;
    cfg.initialBandwidthBps = 250000.0;
    cfg.probeStepFactor = 0.08;
    cfg.probeIntervalMs = 500;
    cfg.probeStableAckCount = 4;
    BandwidthEstimator est(cfg);

    qint64 t = 0;
    double prevEst = est.estimatedBandwidthBps();
    for (quint32 i = 1; i <= 12; ++i)
    {
        est.onFrameSent(i, 50000, 4, 50000, t);
        est.onFrameAcknowledged(i, t + 180, 0);
        const double curEst = est.estimatedBandwidthBps();
        if (i <= 6)
        {
            QVERIFY(curEst >= prevEst);
        }
        QVERIFY(curEst <= prevEst * 1.12); // Upward discovery EMA + at most one +8% probe step
        prevEst = curEst;
        t += 600;
    }

    QVERIFY(est.estimatedBandwidthBps() > 300000.0);
    QVERIFY(!est.isCongested());
    QVERIFY(qFuzzyCompare(est.availableBandwidthBps(), est.estimatedBandwidthBps() * 0.8));
}

void AdaptiveCoreModulesTest::bandwidthEstimatorFastBackoffOnQueueGrowth()
{
    BandwidthEstimator::Config cfg;
    cfg.initialBandwidthBps = 1000000.0;
    cfg.queueGrowthThresholdBytes = 65536;
    cfg.backoffFactor = 0.7;
    cfg.backoffCooldownMs = 300;
    BandwidthEstimator est(cfg);

    // Just-written frame pendingBytesAfterWrite == wireBytes -> preExistingPending == 0 -> no congestion.
    est.onFrameSent(1, 100000, 5, 100000, 100);
    QVERIFY(!est.isCongested());
    QCOMPARE(est.estimatedBandwidthBps(), 1000000.0);

    est.onBackpressureSample(0, 150);
    QVERIFY(!est.isCongested());

    est.onBackpressureSample(80000, 250);
    QVERIFY(est.isCongested());
    QVERIFY(qFuzzyCompare(est.estimatedBandwidthBps(), 700000.0));

    // After backoffCooldownMs (300 ms), another backlog sample drops by 0.7 again.
    est.onBackpressureSample(150000, 600);
    QVERIFY(est.isCongested());
    QVERIFY(qFuzzyCompare(est.estimatedBandwidthBps(), 490000.0));
}

void AdaptiveCoreModulesTest::bandwidthEstimatorFastBackoffOnRttSpike()
{
    BandwidthEstimator::Config cfg;
    cfg.initialBandwidthBps = 1000000.0;
    cfg.backoffFactor = 0.7;
    BandwidthEstimator est(cfg);

    qint64 t = 0;
    for (quint32 i = 1; i <= 4; ++i)
    {
        est.onFrameSent(i, 40000, 4, 40000, t);
        est.onFrameAcknowledged(i, t + 40, 0);
        t += 100;
    }
    QVERIFY(!est.isCongested());
    const double beforeSpike = est.estimatedBandwidthBps();
    QCOMPARE(est.minRttMs(), 40.0);

    est.onFrameSent(5, 40000, 4, 40000, t);
    est.onFrameAcknowledged(5, t + 220, 0);

    QVERIFY(est.isCongested());
    QVERIFY(qFuzzyCompare(est.estimatedBandwidthBps(), beforeSpike * 0.7));
    QCOMPARE(est.minRttMs(), 40.0);
    QVERIFY(est.smoothedRttMs() > 40.0);
}

void AdaptiveCoreModulesTest::bandwidthEstimatorFastBackoffOnAckTimeout()
{
    BandwidthEstimator::Config cfg;
    cfg.initialBandwidthBps = 1000000.0;
    cfg.timeoutBackoffFactor = 0.5;
    BandwidthEstimator est(cfg);

    est.onFrameSent(1, 60000, 4, 60000, 1000);
    est.onAckTimeout(1, 1500, 0);

    QVERIFY(est.isCongested());
    QCOMPARE(est.estimatedBandwidthBps(), 500000.0);

    // Delayed ACK for the timed-out frame updates RTT/throughput without clearing congestion or probing upward.
    est.onFrameAcknowledged(1, 3000, 0);
    QVERIFY(est.isCongested());
    QVERIFY(est.estimatedBandwidthBps() <= 500000.0);
    QCOMPARE(est.smoothedRttMs(), 2000.0);
    QCOMPARE(est.observedThroughputBps(), 30000.0);
}

void AdaptiveCoreModulesTest::bandwidthEstimatorRecoveryAfterCongestion()
{
    BandwidthEstimator::Config cfg;
    cfg.initialBandwidthBps = 1000000.0;
    cfg.timeoutBackoffFactor = 0.5;
    BandwidthEstimator est(cfg);

    est.onFrameSent(1, 60000, 4, 60000, 100);
    est.onAckTimeout(1, 1200, 0);
    QVERIFY(est.isCongested());
    const double trough = est.estimatedBandwidthBps();
    QCOMPARE(trough, 500000.0);

    est.onBackpressureSample(0, 1400);
    qint64 t = 1500;
    for (quint32 i = 2; i <= 9; ++i)
    {
        est.onFrameSent(i, 50000, 4, 50000, t);
        est.onFrameAcknowledged(i, t + 40, 0);
        t += 600;
    }

    QVERIFY(!est.isCongested());
    QVERIFY(est.estimatedBandwidthBps() > trough);
}

void AdaptiveCoreModulesTest::bandwidthEstimatorEdgeCasesAndBoundedMap()
{
    BandwidthEstimator::Config cfg;
    cfg.initialBandwidthBps = 300000.0;
    cfg.maxInFlightTracked = 64;
    BandwidthEstimator est(cfg);

    // Unknown frameId ACK -> no-op
    est.onFrameAcknowledged(9999, 500, 0);
    QCOMPARE(est.estimatedBandwidthBps(), 300000.0);

    // Duplicate ACK -> second call is a no-op
    est.onFrameSent(10, 40000, 2, 40000, 100);
    est.onFrameAcknowledged(10, 150, 0);
    const double afterFirstAck = est.estimatedBandwidthBps();
    est.onFrameAcknowledged(10, 200, 0);
    QCOMPARE(est.estimatedBandwidthBps(), afterFirstAck);

    // Out-of-order ACKs and nowMs <= sendTimeMs (0 ms / negative delta)
    est.onFrameSent(11, 30000, 2, 30000, 1000);
    est.onFrameSent(12, 30000, 2, 30000, 1010);
    est.onFrameAcknowledged(12, 1010, 0); // 0 ms delta
    est.onFrameAcknowledged(11, 995, 0);  // negative delta (-5 ms)
    QVERIFY(std::isfinite(est.estimatedBandwidthBps()));
    QVERIFY(std::isfinite(est.smoothedRttMs()));
    QVERIFY(est.estimatedBandwidthBps() >= cfg.minBandwidthBps
            && est.estimatedBandwidthBps() <= cfg.maxBandwidthBps);

    // Zero / negative wireBytes ignored
    est.onFrameSent(13, 0, 1, 0, 1100);
    est.onFrameSent(14, -100, 1, 0, 1105);
    QCOMPARE(est.inFlightCount(), 0);

    // 200 unacked frames -> bounded by maxInFlightTracked (64)
    for (quint32 i = 100; i < 300; ++i)
    {
        est.onFrameSent(i, 20000, 1, 20000, 2000 + static_cast<qint64>(i));
    }
    QVERIFY(est.inFlightCount() <= 64);

    est.reset();
    QCOMPARE(est.inFlightCount(), 0);
    QCOMPARE(est.estimatedBandwidthBps(), 300000.0);
    QCOMPARE(est.observedThroughputBps(), 0.0);
    QCOMPARE(est.smoothedRttMs(), 0.0);
    QCOMPARE(est.minRttMs(), 0.0);
    QVERIFY(!est.isCongested());
}

QTEST_MAIN(AdaptiveCoreModulesTest)
#include "AdaptiveCoreModulesTest.moc"
