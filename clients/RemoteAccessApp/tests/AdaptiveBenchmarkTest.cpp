#include <QtTest>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>

#include "Network/Protocol/ScreenFramePacketizer.h"
#include "Streaming/AdaptiveStreamController.h"
#include "Streaming/BandwidthEstimator.h"
#include "Streaming/MotionDetector.h"
#include "Streaming/ScreenEncoder.h"
#include "Streaming/ScreenStreamSender.h"
#include "Streaming/StreamMetrics.h"
#include "Streaming/StreamOptimizer.h"
#include "e2e/E2eFrameOracle.h"
#include "e2e/E2eRelayHarness.h"

#include <algorithm>
#include <cmath>

namespace {

struct ScenarioMetrics
{
    QString scenarioName;
    int framesOffered = 0;
    int framesSent = 0;
    int framesDropped = 0;
    int overBudgetFrames = 0;
    int configSwitches = 0;
    double avgQuality = 0.0;
    double avgScale = 0.0;
    double avgPsnrDb = 0.0;
    double actualFps = 0.0;
    double avgLatencyMs = 0.0;
    double avgEncodeMs = 0.0;
    double avgEncodeAttempts = 0.0;
    double avgBandwidthUsedBps = 0.0;
};

// Faithful simulation of the pre-adaptive baseline (commit 83a9060):
// Fixed 1000 Kbps (125,000 B/s), fixed q=60, /5 motion burst (24,960 B) & /2 refresh burst (62,460 B),
// ScreenEncoder::encodeWithinLimit (width/2, height/2 halving), 100ms tick, 250ms minor, 1200ms static refresh.
ScenarioMetrics runBaselineSimulation(const QString &name,
                                      const QList<QImage> &frames,
                                      double linkBandwidthBps,
                                      double baseRttMs,
                                      double jitterMs = 0.0,
                                      qint64 durationMs = 3000)
{
    ScenarioMetrics m;
    m.scenarioName = name;

    constexpr double fixedWireBps = 125000.0; // Fixed 1000 Kbps
    constexpr qsizetype maxWireBytes = 256 * 1024;
    constexpr qsizetype overhead = 40;

    QImage lastSentFrame;
    qint64 lastSendTimeMs = -1200;
    qint64 nextCaptureDelayMs = 0;
    qint64 ackFreeAtMs = 0;

    double sumQ = 0.0;
    double sumScale = 0.0;
    double sumPsnr = 0.0;
    double sumLatency = 0.0;
    double sumEncodeMs = 0.0;
    double sumAttempts = 0.0;
    qint64 totalWireBytes = 0;
    double prevScale = -1.0;

    int frameIdx = 0;
    for (qint64 nowMs = 0; nowMs < durationMs; nowMs += 100) {
        if (nowMs < ackFreeAtMs)
            continue;
        const qint64 elapsedSinceSend = (lastSendTimeMs >= 0) ? (nowMs - lastSendTimeMs) : 1200;
        if (elapsedSinceSend < nextCaptureDelayMs)
            continue;

        const QImage &frame = frames[frameIdx % frames.size()];
        ++frameIdx;
        ++m.framesOffered;

        const auto change = ScreenStreamSender::classifyFrameChange(frame, lastSentFrame);
        if (change == ScreenStreamSender::FrameChange::None && elapsedSinceSend < 1200)
            continue;
        if (change == ScreenStreamSender::FrameChange::Minor && elapsedSinceSend < 250)
            continue;

        const qsizetype burst = static_cast<qsizetype>(
            fixedWireBps / (change == ScreenStreamSender::FrameChange::Significant ? 5.0 : 2.0));
        const qsizetype jpegBudget = std::min(burst, maxWireBytes) - overhead;

        QElapsedTimer encTimer;
        encTimer.start();
        int attempts = 0;
        QImage candidate = frame;
        QByteArray chosenJpeg;
        QSize chosenSize;
        while (!candidate.isNull()) {
            ++attempts;
            QByteArray j = ScreenEncoder::encodeJpeg(candidate, 60);
            if (j.isEmpty())
                break;
            if (j.size() <= jpegBudget) {
                chosenJpeg = std::move(j);
                chosenSize = candidate.size();
                break;
            }
            if (candidate.width() <= 1 || candidate.height() <= 1)
                break;
            candidate = candidate.scaled(qMax(1, candidate.width() / 2),
                                         qMax(1, candidate.height() / 2),
                                         Qt::IgnoreAspectRatio,
                                         Qt::SmoothTransformation);
        }
        const double encMs = std::max(1.0, static_cast<double>(encTimer.elapsed()));

        if (chosenJpeg.isEmpty()) {
            ++m.framesDropped;
            continue;
        }

        const qsizetype wireBytes = chosenJpeg.size() + overhead;
        if (wireBytes > maxWireBytes)
            ++m.overBudgetFrames;

        const double scale = static_cast<double>(chosenSize.width()) / static_cast<double>(frame.width());
        if (prevScale > 0.0 && std::abs(scale - prevScale) > 1e-4)
            ++m.configSwitches;
        prevScale = scale;

        const QImage decoded = QImage::fromData(chosenJpeg, "JPEG");
        const double psnr = E2eOracle::computePsnr(frame, decoded);
        const double effectiveLinkBps = std::max(1000.0, linkBandwidthBps);
        const double txMs = (static_cast<double>(wireBytes) * 1000.0) / effectiveLinkBps;
        const double jit = (jitterMs > 0.0) ? ((m.framesSent % 3) * 0.5 * jitterMs) : 0.0;
        const double frameRttMs = baseRttMs + jit + txMs;

        ++m.framesSent;
        totalWireBytes += wireBytes;
        sumQ += 60.0;
        sumScale += scale;
        sumPsnr += psnr;
        sumLatency += (encMs + frameRttMs);
        sumEncodeMs += encMs;
        sumAttempts += attempts;

        lastSentFrame = frame;
        lastSendTimeMs = nowMs + static_cast<qint64>(encMs);
        nextCaptureDelayMs = static_cast<qint64>((wireBytes * 1000.0 + fixedWireBps - 1.0) / fixedWireBps);
        ackFreeAtMs = lastSendTimeMs + static_cast<qint64>(frameRttMs);
    }

    if (m.framesSent > 0) {
        m.avgQuality = sumQ / m.framesSent;
        m.avgScale = sumScale / m.framesSent;
        m.avgPsnrDb = sumPsnr / m.framesSent;
        m.avgLatencyMs = sumLatency / m.framesSent;
        m.avgEncodeMs = sumEncodeMs / m.framesSent;
        m.avgEncodeAttempts = sumAttempts / m.framesSent;
    }
    m.actualFps = (m.framesSent * 1000.0) / static_cast<double>(durationMs);
    m.avgBandwidthUsedBps = (totalWireBytes * 1000.0) / static_cast<double>(durationMs);
    return m;
}

ScenarioMetrics runAdaptiveSimulation(const QString &name,
                                      const QList<QImage> &frames,
                                      double initialBwBps,
                                      double linkBandwidthBps,
                                      double baseRttMs,
                                      double jitterMs = 0.0,
                                      qint64 durationMs = 3000,
                                      std::optional<double> cpuLoad = std::nullopt,
                                      double midShiftLinkBps = -1.0,
                                      qint64 midShiftAtMs = 1500,
                                      bool induceAckStall = false)
{
    ScenarioMetrics m;
    m.scenarioName = name;

    MotionDetector motionDetector;
    BandwidthEstimator::Config bwCfg;
    bwCfg.initialBandwidthBps = initialBwBps;
    bwCfg.probeIntervalMs = 200;
    bwCfg.probeStableAckCount = 2;
    BandwidthEstimator estimator(bwCfg);

    AdaptiveStreamController::Config ctrlCfg;
    ctrlCfg.slowLoopIntervalMs = 600;
    ctrlCfg.minEventSlowLoopIntervalMs = 150;
    AdaptiveStreamController controller(ctrlCfg);

    qint64 lastSendTimeMs = -1000;
    qint64 nextCaptureDelayMs = 0;
    qint64 ackFreeAtMs = 0;
    quint32 inFlightFrameId = 0;
    double lastAckRttMs = -1.0;

    double sumQ = 0.0;
    double sumScale = 0.0;
    double sumPsnr = 0.0;
    double sumLatency = 0.0;
    double sumEncodeMs = 0.0;
    double sumAttempts = 0.0;
    qint64 totalWireBytes = 0;

    int frameIdx = 0;
    for (qint64 nowMs = 0; nowMs < durationMs; nowMs += 20) {
        const double currentLinkBps = (midShiftLinkBps > 0.0 && nowMs >= midShiftAtMs)
                ? midShiftLinkBps
                : linkBandwidthBps;

        if (inFlightFrameId != 0 && nowMs >= ackFreeAtMs) {
            estimator.onFrameAcknowledged(inFlightFrameId, nowMs, 0);
            inFlightFrameId = 0;
        }
        if (inFlightFrameId != 0) {
            if (induceAckStall && (nowMs - lastSendTimeMs) >= 1500) {
                estimator.onAckTimeout(inFlightFrameId, nowMs - lastSendTimeMs, 0);
                lastAckRttMs = static_cast<double>(nowMs - lastSendTimeMs);
                inFlightFrameId = 0;
            } else {
                continue;
            }
        }

        const qint64 elapsedSinceSend = (lastSendTimeMs >= 0) ? (nowMs - lastSendTimeMs) : nextCaptureDelayMs;
        if (elapsedSinceSend < nextCaptureDelayMs)
            continue;

        const QImage &frame = frames[frameIdx % frames.size()];
        ++frameIdx;
        ++m.framesOffered;

        const MotionResult mot = motionDetector.analyze(frame);

        AdaptiveStreamController::FrameInput in;
        in.frameId = static_cast<quint64>(m.framesOffered);
        in.frame = frame;
        in.nowMs = nowMs;
        in.motionScore = mot.smoothedScore;
        in.wQ = mot.wQ;
        in.wR = mot.wR;
        in.wP = mot.wP;
        in.estimatedBandwidthBps = estimator.estimatedBandwidthBps();
        in.availableBandwidthBps = estimator.availableBandwidthBps();
        in.observedThroughputBps = estimator.observedThroughputBps();
        in.smoothedRttMs = estimator.smoothedRttMs();
        in.minRttMs = estimator.minRttMs();
        in.networkCongested = estimator.isCongested();
        in.pendingBytes = 0;
        in.ackLatencyMs = lastAckRttMs;
        in.ackEnabled = true;
        in.cpuUsagePercent = cpuLoad;

        const AdaptiveStreamController::FrameDecision dec = controller.processFrame(in);
        if (dec.configChanged && m.framesOffered > 1)
            ++m.configSwitches;

        if (!dec.shouldSend || dec.dropped || dec.jpeg.isEmpty()) {
            ++m.framesDropped;
            lastSendTimeMs = nowMs;
            nextCaptureDelayMs = 100;
            continue;
        }

        if (dec.wireBytes > controller.effectiveMaxWireBytes() || dec.wireBytes > dec.frameByteBudget) {
            ++m.overBudgetFrames;
        }

        const QImage decoded = QImage::fromData(dec.jpeg, "JPEG");
        const double psnr = E2eOracle::computePsnr(frame, decoded);
        const double effectiveLinkBps = std::max(1000.0, currentLinkBps);
        const double txMs = (static_cast<double>(dec.wireBytes) * 1000.0) / effectiveLinkBps;
        const double jit = (jitterMs > 0.0) ? ((m.framesSent % 3) * 0.5 * jitterMs) : 0.0;
        const double frameRttMs = (induceAckStall && m.framesSent == 2)
                ? 1600.0
                : (baseRttMs + jit + txMs);

        ++m.framesSent;
        totalWireBytes += dec.wireBytes;
        sumQ += dec.qualityUsed;
        sumScale += dec.scaleUsed;
        sumPsnr += psnr;
        sumLatency += (dec.encodeTimeMs + frameRttMs);
        sumEncodeMs += std::max<qint64>(1, dec.encodeTimeMs);
        sumAttempts += dec.encodeRetryCount + 1;

        const qint64 sentAt = nowMs + std::max<qint64>(1, dec.encodeTimeMs);
        inFlightFrameId = static_cast<quint32>(m.framesOffered);
        estimator.onFrameSent(inFlightFrameId, dec.wireBytes, dec.encodeTimeMs, 0, sentAt);
        lastSendTimeMs = sentAt;
        lastAckRttMs = frameRttMs;
        nextCaptureDelayMs = AdaptiveStreamController::computeNextFrameDelayMs(
            dec.wireBytes,
            dec.encodeTimeMs,
            0.0,
            estimator.availableBandwidthBps(),
            dec.targetFps,
            false);
        ackFreeAtMs = sentAt + static_cast<qint64>(frameRttMs);
    }

    if (m.framesSent > 0) {
        m.avgQuality = sumQ / m.framesSent;
        m.avgScale = sumScale / m.framesSent;
        m.avgPsnrDb = sumPsnr / m.framesSent;
        m.avgLatencyMs = sumLatency / m.framesSent;
        m.avgEncodeMs = sumEncodeMs / m.framesSent;
        m.avgEncodeAttempts = sumAttempts / m.framesSent;
    }
    m.actualFps = (m.framesSent * 1000.0) / static_cast<double>(durationMs);
    m.avgBandwidthUsedBps = (totalWireBytes * 1000.0) / static_cast<double>(durationMs);
    return m;
}

} // namespace

class AdaptiveBenchmarkTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void scenario01_HighSpeedLan();
    void scenario02_HighRttVps();
    void scenario03_LowBandwidth();
    void scenario04_SuddenBandwidthDrop();
    void scenario05_BandwidthRecovery();
    void scenario06_JitterAndPacketLoss();
    void scenario07_StaticTextHeavyScreen();
    void scenario08_ScrollingAndWindowDrag();
    void scenario09_ContinuousVideoMotion();
    void scenario10_AckTimeoutAndStall();
    void scenario11_CpuOverload();
    void scenario12_InfeasibleConfigurationFallback();
    void scenario13_ProtocolPayloadLimit();
    void cleanupTestCase();

private:
    QList<QImage> m_staticTextFrames;
    QList<QImage> m_scrollingFrames;
    QList<QImage> m_videoFrames;
    QList<QImage> m_noiseFrames;
    QList<QPair<ScenarioMetrics, ScenarioMetrics>> m_results;
};

void AdaptiveBenchmarkTest::initTestCase()
{
    E2eOracle::StaticTextFrameGenerator textGen(QSize(1280, 720), true);
    E2eOracle::ScrollingUiFrameGenerator scrollGen(QSize(1280, 720), 28);
    E2eOracle::HighMotionVideoFrameGenerator videoGen(QSize(1280, 720));
    E2eOracle::WorstCaseEntropyNoiseGenerator noiseGen(QSize(1920, 1080), 77u);

    for (int i = 0; i < 6; ++i) {
        m_staticTextFrames.append(textGen.nextFrame());
        m_scrollingFrames.append(scrollGen.nextFrame());
        m_videoFrames.append(videoGen.nextFrame());
    }
    for (int i = 0; i < 3; ++i) {
        m_noiseFrames.append(noiseGen.nextFrame());
    }
}

void AdaptiveBenchmarkTest::scenario01_HighSpeedLan()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("01. High-Speed LAN (24 Mbps, 2ms)"), m_scrollingFrames, 3000000.0, 2.0, 0.0, 2000);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("01. High-Speed LAN (24 Mbps, 2ms)"), m_scrollingFrames, 3000000.0, 3000000.0, 2.0, 0.0, 2000);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY2(adap.avgScale > base.avgScale,
             qPrintable(QStringLiteral("Adaptive scale %1 vs baseline %2").arg(adap.avgScale).arg(base.avgScale)));
    QVERIFY2(adap.avgPsnrDb > base.avgPsnrDb,
             qPrintable(QStringLiteral("Adaptive PSNR %1 vs baseline %2").arg(adap.avgPsnrDb).arg(base.avgPsnrDb)));
    QVERIFY2(adap.actualFps >= base.actualFps,
             qPrintable(QStringLiteral("Adaptive FPS %1 vs baseline %2").arg(adap.actualFps).arg(base.actualFps)));
}

void AdaptiveBenchmarkTest::scenario02_HighRttVps()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("02. High-RTT Australia VPS (3 Mbps, 160ms)"), m_scrollingFrames, 375000.0, 160.0, 0.0, 2400);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("02. High-RTT Australia VPS (3 Mbps, 160ms)"), m_scrollingFrames, 350000.0, 375000.0, 160.0, 0.0, 2400);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY(adap.framesSent >= 5);
    QVERIFY2(adap.avgScale > base.avgScale,
             qPrintable(QStringLiteral("VPS adaptive scale %1 vs baseline %2").arg(adap.avgScale).arg(base.avgScale)));
    QVERIFY2(adap.avgPsnrDb > base.avgPsnrDb,
             qPrintable(QStringLiteral("VPS adaptive PSNR %1 vs baseline %2").arg(adap.avgPsnrDb).arg(base.avgPsnrDb)));
}

void AdaptiveBenchmarkTest::scenario03_LowBandwidth()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("03. Low Bandwidth (750 Kbps, 40ms)"), m_scrollingFrames, 93750.0, 40.0, 0.0, 2400);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("03. Low Bandwidth (750 Kbps, 40ms)"), m_scrollingFrames, 93750.0, 93750.0, 40.0, 0.0, 2400);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY(adap.framesSent >= 3);
    QVERIFY(adap.avgBandwidthUsedBps <= 93750.0 * 1.05);
    QVERIFY2(adap.avgScale > base.avgScale,
             qPrintable(QStringLiteral("Low-BW adaptive scale %1 vs baseline %2").arg(adap.avgScale).arg(base.avgScale)));
}

void AdaptiveBenchmarkTest::scenario04_SuddenBandwidthDrop()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("04. Sudden BW Drop (4 Mbps -> 750 Kbps)"), m_scrollingFrames, 93750.0, 50.0, 0.0, 3200);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("04. Sudden BW Drop (4 Mbps -> 750 Kbps)"),
        m_scrollingFrames, 500000.0, 500000.0, 30.0, 0.0, 3200, std::nullopt, 93750.0, 800);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY(adap.framesSent >= 4);
}

void AdaptiveBenchmarkTest::scenario05_BandwidthRecovery()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("05. BW Recovery (750 Kbps -> 4 Mbps)"), m_scrollingFrames, 500000.0, 25.0, 0.0, 2800);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("05. BW Recovery (750 Kbps -> 4 Mbps)"),
        m_scrollingFrames, 93750.0, 93750.0, 40.0, 0.0, 2800, std::nullopt, 500000.0, 800);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY(adap.framesSent >= 4);
}

void AdaptiveBenchmarkTest::scenario06_JitterAndPacketLoss()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("06. Jitter & Loss (1.5 Mbps, 60+-50ms)"), m_scrollingFrames, 187500.0, 60.0, 50.0, 2400);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("06. Jitter & Loss (1.5 Mbps, 60+-50ms)"), m_scrollingFrames, 187500.0, 187500.0, 60.0, 50.0, 2400);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY(adap.configSwitches <= 6); // Hysteresis prevents oscillation
}

void AdaptiveBenchmarkTest::scenario07_StaticTextHeavyScreen()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("07. Static Text-Heavy Screen (1280x720)"), m_staticTextFrames, 750000.0, 20.0, 0.0, 2400);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("07. Static Text-Heavy Screen (1280x720)"), m_staticTextFrames, 750000.0, 750000.0, 20.0, 0.0, 2400);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QCOMPARE(adap.avgScale, 1.0); // Full 1.0 resolution preserved for text
    QVERIFY2(adap.avgQuality > base.avgQuality,
             qPrintable(QStringLiteral("Adaptive static text Q=%1 vs baseline Q=%2")
                            .arg(adap.avgQuality)
                            .arg(base.avgQuality)));
    QVERIFY2(adap.avgPsnrDb > base.avgPsnrDb,
             qPrintable(QStringLiteral("Adaptive static text PSNR=%1 vs baseline PSNR=%2")
                            .arg(adap.avgPsnrDb)
                            .arg(base.avgPsnrDb)));
}

void AdaptiveBenchmarkTest::scenario08_ScrollingAndWindowDrag()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("08. Scrolling & Window Drag (2.5 Mbps)"), m_scrollingFrames, 312500.0, 25.0, 0.0, 2000);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("08. Scrolling & Window Drag (2.5 Mbps)"), m_scrollingFrames, 312500.0, 312500.0, 25.0, 0.0, 2000);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY2(adap.avgScale >= 0.75 && adap.avgScale > base.avgScale,
             qPrintable(QStringLiteral("Scrolling adaptive scale %1 vs baseline %2")
                            .arg(adap.avgScale)
                            .arg(base.avgScale)));
}

void AdaptiveBenchmarkTest::scenario09_ContinuousVideoMotion()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("09. Continuous Video Motion (16 Mbps LAN)"), m_videoFrames, 2000000.0, 5.0, 0.0, 2000);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("09. Continuous Video Motion (16 Mbps LAN)"), m_videoFrames, 2000000.0, 2000000.0, 5.0, 0.0, 2000);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY2(adap.actualFps > base.actualFps,
             qPrintable(QStringLiteral("Video adaptive FPS %1 vs baseline %2")
                            .arg(adap.actualFps)
                            .arg(base.actualFps)));
}

void AdaptiveBenchmarkTest::scenario10_AckTimeoutAndStall()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("10. ACK Stall & Recovery (1.5s stall)"), m_scrollingFrames, 250000.0, 40.0, 0.0, 3000);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("10. ACK Stall & Recovery (1.5s stall)"),
        m_scrollingFrames, 250000.0, 250000.0, 40.0, 0.0, 3000, std::nullopt, -1.0, 1500, true);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY(adap.framesSent >= 3);
}

void AdaptiveBenchmarkTest::scenario11_CpuOverload()
{
    const auto normal = runAdaptiveSimulation(
        QStringLiteral("11a. Normal CPU (25% CPU Load)"),
        m_videoFrames, 2000000.0, 2000000.0, 5.0, 0.0, 2000, 25.0);
    const auto base = runBaselineSimulation(
        QStringLiteral("11. CPU Overload (92% CPU Load)"), m_videoFrames, 2000000.0, 5.0, 0.0, 2000);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("11. CPU Overload (92% CPU Load)"),
        m_videoFrames, 2000000.0, 2000000.0, 5.0, 0.0, 2000, 92.0);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY2(adap.actualFps < normal.actualFps,
             qPrintable(QStringLiteral("Overload FPS %1 vs normal %2").arg(adap.actualFps).arg(normal.actualFps)));
}

void AdaptiveBenchmarkTest::scenario12_InfeasibleConfigurationFallback()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("12. Infeasible Budget Fallback (16 Kbps)"), m_noiseFrames, 2000.0, 50.0, 0.0, 1000);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("12. Infeasible Budget Fallback (16 Kbps)"), m_noiseFrames, 2000.0, 2000.0, 50.0, 0.0, 1000);
    m_results.append({base, adap});

    // Safe drop when no configuration fits budget: zero over-budget frames emitted
    QCOMPARE(adap.overBudgetFrames, 0);
    QCOMPARE(adap.framesSent, 0);
    QVERIFY(adap.framesDropped > 0);
}

void AdaptiveBenchmarkTest::scenario13_ProtocolPayloadLimit()
{
    const auto base = runBaselineSimulation(
        QStringLiteral("13. Protocol Payload Limit (256 KiB Cap)"), m_noiseFrames, 5000000.0, 5.0, 0.0, 1200);
    const auto adap = runAdaptiveSimulation(
        QStringLiteral("13. Protocol Payload Limit (256 KiB Cap)"), m_noiseFrames, 5000000.0, 5000000.0, 5.0, 0.0, 1200);
    m_results.append({base, adap});

    QCOMPARE(adap.overBudgetFrames, 0);
    QVERIFY(adap.framesSent >= 1);
}

void AdaptiveBenchmarkTest::cleanupTestCase()
{
    qInfo().noquote() << "\n================ ADAPTIVE STREAMING 13-SCENARIO BENCHMARK REPORT ================";
    qInfo().noquote() << QStringLiteral("| Scenario | Mode | FPS | Scale(r) | Qual(q) | PSNR(dB) | Latency(ms) | Enc(ms) | Tries | Switches | OverBudget |");
    qInfo().noquote() << QStringLiteral("|---|---|---|---|---|---|---|---|---|---|---|");
    for (const auto &pair : m_results) {
        const auto &b = pair.first;
        const auto &a = pair.second;
        qInfo().noquote() << QStringLiteral("| %1 | Baseline | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 | %10 |")
                                 .arg(b.scenarioName, -42)
                                 .arg(b.actualFps, 5, 'f', 2)
                                 .arg(b.avgScale, 4, 'f', 2)
                                 .arg(b.avgQuality, 4, 'f', 1)
                                 .arg(b.avgPsnrDb, 5, 'f', 2)
                                 .arg(b.avgLatencyMs, 6, 'f', 1)
                                 .arg(b.avgEncodeMs, 5, 'f', 1)
                                 .arg(b.avgEncodeAttempts, 4, 'f', 2)
                                 .arg(b.configSwitches)
                                 .arg(b.overBudgetFrames);
        qInfo().noquote() << QStringLiteral("| %1 | Adaptive | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 | %10 |")
                                 .arg(a.scenarioName, -42)
                                 .arg(a.actualFps, 5, 'f', 2)
                                 .arg(a.avgScale, 4, 'f', 2)
                                 .arg(a.avgQuality, 4, 'f', 1)
                                 .arg(a.avgPsnrDb, 5, 'f', 2)
                                 .arg(a.avgLatencyMs, 6, 'f', 1)
                                 .arg(a.avgEncodeMs, 5, 'f', 1)
                                 .arg(a.avgEncodeAttempts, 4, 'f', 2)
                                 .arg(a.configSwitches)
                                 .arg(a.overBudgetFrames);
    }
    qInfo().noquote() << "=================================================================================";
}

QTEST_MAIN(AdaptiveBenchmarkTest)
#include "AdaptiveBenchmarkTest.moc"
