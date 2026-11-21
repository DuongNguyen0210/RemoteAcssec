#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QObject>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>

#include "E2eFrameOracle.h"
#include "E2eRelayHarness.h"

#include "Network/Protocol/ProtocolConstants.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Protocol/RelayAuthPayload.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include "Network/Protocol/ScreenFramePacketizer.h"
#include "Network/Relay/AdminSessionController.h"
#include "Network/Relay/ChildSessionController.h"
#include "Network/Relay/StaticRelayEndpointProvider.h"
#include "Streaming/MotionDetector.h"
#include "Streaming/ScreenEncoder.h"
#include "Streaming/ScreenStreamSender.h"
#include "Streaming/StreamMetrics.h"

#include <functional>

namespace {

struct MockM3Bridge
{
    void setFrameSource(std::function<QImage()>) {}
    void setCpuUsageSource(std::function<double()>) {}
    void screenFrameAcknowledged(quint32, qint64, qsizetype) {}
    void screenFrameAckStalled(quint32, qint64) {}
};

static_assert(E2eOracle::detail::HasSetFrameSource<MockM3Bridge>::value,
              "HasSetFrameSource should detect setFrameSource");
static_assert(E2eOracle::detail::HasSetCpuUsageSource<MockM3Bridge>::value,
              "HasSetCpuUsageSource should detect setCpuUsageSource");
static_assert(E2eOracle::detail::HasM3AckTelemetrySignals<MockM3Bridge>::value,
              "HasM3AckTelemetrySignals should detect M3 ACK signals");

struct MissingM3Bridge
{
};

static_assert(!E2eOracle::detail::HasSetFrameSource<MissingM3Bridge>::value,
              "HasSetFrameSource should be false when missing");
static_assert(!E2eOracle::detail::HasSetCpuUsageSource<MissingM3Bridge>::value,
              "HasSetCpuUsageSource should be false when missing");
static_assert(!E2eOracle::detail::HasM3AckTelemetrySignals<MissingM3Bridge>::value,
              "HasM3AckTelemetrySignals should be false when missing");

bool waitUntil(const std::function<bool()> &predicate, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return predicate();
}

} // namespace

class E2eHarnessSelfTest : public QObject
{
    Q_OBJECT

private slots:
    void staticTextGeneratorIsDeterministicAndSupportsCursorBlink();
    void scrollingUiGeneratorAdvancesDeterministically();
    void highMotionVideoGeneratorExceedsMotionThreshold();
    void worstCaseEntropyNoiseGeneratorExceedsPayloadAndCompressesWithinLimit();
    void markerColorStepGeneratorSurvivesJpegAndDownscale();
    void psnrOracleMeasuresExactAndDegradedFidelity();
    void dqtQualityEstimatorMatchesScreenEncoderAcrossQualities();
    void streamMetricsLogCollectorParsesAll15FieldsAndHandlesQDebug();
    void guiResponsivenessProbeMeasuresLoopDelays();
    void sfinaeSeamDetectorsCompileAndMatchBranchState();
    void relayHandshakeOrderingAndFrameDeliveryWork();
    void relayValidatesWireLimitsAndInputPackets();
    void relayEnforcesBandwidthRttAndJitter();
    void relayEnforcesChunkDropAndLossPercent();
    void relayEnforcesAckDelayDropAcksAndUnwritableReservation();
    void relayEnforcesReadBufferBackpressureAndScheduledProfileChange();
    void relayEnforcesSubMillisecondBandwidthShaping();
    void relayIgnoresStaleSessionPacketsAfterDisconnect();
    void relayForwardsInputPacketsAcrossActiveSession();
};

void E2eHarnessSelfTest::staticTextGeneratorIsDeterministicAndSupportsCursorBlink()
{
    E2eOracle::StaticTextFrameGenerator genA(QSize(1920, 1080), false);
    E2eOracle::StaticTextFrameGenerator genB(QSize(1920, 1080), false);

    const QImage a0 = genA.nextFrame();
    const QImage a1 = genA.nextFrame();
    const QImage b0 = genB.nextFrame();

    QCOMPARE(a0.size(), QSize(1920, 1080));
    QCOMPARE(a0.format(), QImage::Format_RGB32);
    QCOMPARE(a0, a1);
    QCOMPARE(a0, b0);

    E2eOracle::StaticTextFrameGenerator blinkGen(QSize(1920, 1080), true);
    MotionDetector detector;
    const auto r0 = detector.analyze(blinkGen.nextFrame());
    QVERIFY(r0.valid);
    for (int i = 0; i < 6; ++i) {
        const auto res = detector.analyze(blinkGen.nextFrame());
        QVERIFY(res.valid);
        QVERIFY2(res.rawScore < 0.02, qPrintable(QString::number(res.rawScore)));
        QVERIFY(res.smoothedScore < 0.02);
    }

    const QImage f0 = blinkGen.nextFrame();
    const QImage f1 = blinkGen.nextFrame();
    QVERIFY(f0 != f1);
    blinkGen.reset();
    QCOMPARE(blinkGen.frameIndex(), 0);
}

void E2eHarnessSelfTest::scrollingUiGeneratorAdvancesDeterministically()
{
    E2eOracle::ScrollingUiFrameGenerator genA(QSize(1920, 1080), 24);
    E2eOracle::ScrollingUiFrameGenerator genB(QSize(1920, 1080), 24);

    const QImage a0 = genA.nextFrame();
    const QImage a1 = genA.nextFrame();
    const QImage b0 = genB.nextFrame();
    const QImage b1 = genB.nextFrame();

    QCOMPARE(a0.size(), QSize(1920, 1080));
    QCOMPARE(a0, b0);
    QCOMPARE(a1, b1);
    QVERIFY(a0 != a1);

    MotionDetector detector;
    detector.analyze(a0);
    const auto r1 = detector.analyze(a1);
    QVERIFY(r1.valid);
    QVERIFY2(r1.rawScore > 0.02 && r1.rawScore < 0.95,
             qPrintable(QStringLiteral("scrolling rawScore=%1").arg(r1.rawScore)));

    genA.reset();
    QCOMPARE(genA.nextFrame(), b0);
}

void E2eHarnessSelfTest::highMotionVideoGeneratorExceedsMotionThreshold()
{
    E2eOracle::HighMotionVideoFrameGenerator gen(QSize(1920, 1080));
    MotionDetector detector;

    MotionResult last;
    for (int i = 0; i < 10; ++i) {
        const QImage frame = gen.nextFrame();
        QCOMPARE(frame.size(), QSize(1920, 1080));
        last = detector.analyze(frame);
        QVERIFY(last.valid);
        if (i > 0)
            QVERIFY2(last.rawScore >= 0.60, qPrintable(QStringLiteral("frame %1 raw=%2").arg(i).arg(last.rawScore)));
    }

    QVERIFY2(last.smoothedScore > 0.70, qPrintable(QString::number(last.smoothedScore)));
    QVERIFY2(last.wP > 0.40, qPrintable(QString::number(last.wP)));
}

void E2eHarnessSelfTest::worstCaseEntropyNoiseGeneratorExceedsPayloadAndCompressesWithinLimit()
{
    E2eOracle::WorstCaseEntropyNoiseGenerator genA(QSize(1920, 1080), 42u);
    E2eOracle::WorstCaseEntropyNoiseGenerator genB(QSize(1920, 1080), 42u);
    E2eOracle::WorstCaseEntropyNoiseGenerator genOther(QSize(1920, 1080), 99u);

    const QImage noiseA = genA.nextFrame();
    const QImage noiseB = genB.nextFrame();
    const QImage noiseOther = genOther.nextFrame();

    QCOMPARE(noiseA, noiseB);
    QVERIFY(noiseA != noiseOther);

    const QByteArray rawQ60 = ScreenEncoder::encodeJpeg(noiseA, 60);
    QVERIFY2(rawQ60.size() > 262104,
             qPrintable(QStringLiteral("Expected noise JPEG > 262104 B, got %1").arg(rawQ60.size())));

    const EncodedScreenFrame bounded = ScreenEncoder::encodeWithinLimit(noiseA, 262104);
    QVERIFY(!bounded.jpeg.isEmpty());
    QVERIFY2(bounded.jpeg.size() <= 262104,
             qPrintable(QStringLiteral("Bounded JPEG exceeded 262104 B: %1").arg(bounded.jpeg.size())));
}

void E2eHarnessSelfTest::markerColorStepGeneratorSurvivesJpegAndDownscale()
{
    E2eOracle::MarkerColorStepGenerator gen(QSize(1280, 800));
    const QList<int> steps = {0, 1, 7, 42, 129, 255};

    for (int step : steps) {
        gen.setStep(step);
        const QImage frame = gen.nextFrame();
        const QImage scaled = frame.scaled(640, 400, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const QByteArray jpeg = ScreenEncoder::encodeJpeg(scaled, 30);
        const QImage decoded = QImage::fromData(jpeg, "JPEG");
        QVERIFY(!decoded.isNull());
        QCOMPARE(E2eOracle::MarkerColorStepGenerator::detectMarkerStep(decoded), step);
    }

    QCOMPARE(E2eOracle::MarkerColorStepGenerator::detectMarkerStep(QImage()), -1);
}

void E2eHarnessSelfTest::psnrOracleMeasuresExactAndDegradedFidelity()
{
    E2eOracle::StaticTextFrameGenerator gen(QSize(640, 480), false);
    const QImage ref = gen.nextFrame();

    QCOMPARE(E2eOracle::computePsnr(QImage(), ref), 0.0);
    QCOMPARE(E2eOracle::computePsnr(ref, QImage()), 0.0);
    QCOMPARE(E2eOracle::computePsnr(ref, ref), 100.0);

    const QImage dec90 = QImage::fromData(ScreenEncoder::encodeJpeg(ref, 90), "JPEG");
    const QImage dec60 = QImage::fromData(ScreenEncoder::encodeJpeg(ref, 60), "JPEG");
    const QImage dec30 = QImage::fromData(ScreenEncoder::encodeJpeg(ref, 30), "JPEG");
    const QImage decHalf = dec60.scaled(320, 240, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    const double psnr90 = E2eOracle::computePsnr(ref, dec90);
    const double psnr60 = E2eOracle::computePsnr(ref, dec60);
    const double psnr30 = E2eOracle::computePsnr(ref, dec30);
    const double psnrHalf = E2eOracle::computePsnr(ref, decHalf);

    QVERIFY(psnr90 > psnr60);
    QVERIFY(psnr60 > psnr30);
    QVERIFY(psnr30 > 15.0);
    QVERIFY(psnrHalf > 10.0 && psnrHalf < psnr60);
}

void E2eHarnessSelfTest::dqtQualityEstimatorMatchesScreenEncoderAcrossQualities()
{
    E2eOracle::StaticTextFrameGenerator gen(QSize(320, 240), false);
    const QImage frame = gen.nextFrame();

    const QList<int> qualities = {1, 10, 25, 30, 50, 60, 75, 85, 90, 100};
    for (int q : qualities) {
        const QByteArray jpeg = ScreenEncoder::encodeJpeg(frame, q);
        QCOMPARE(E2eOracle::estimateJpegQualityFromDqt(jpeg), q);
    }

    QCOMPARE(E2eOracle::estimateJpegQualityFromDqt(QByteArray()), -1);
    QCOMPARE(E2eOracle::estimateJpegQualityFromDqt(QByteArray("not-a-jpeg")), -1);
}

void E2eHarnessSelfTest::streamMetricsLogCollectorParsesAll15FieldsAndHandlesQDebug()
{
    {
        E2eOracle::StreamMetricsLogCollector collector(true);

        StreamMetrics metrics(0);
        StreamMetricsSnapshot s1;
        s1.estimatedBandwidthBps = 250000.0;
        s1.observedThroughputBps = 180000.0;
        s1.frameByteBudget = 30000;
        s1.actualFrameBytes = 19500;
        s1.motionScore = 0.25;
        s1.jpegQuality = 78;
        s1.resolution = QSize(1920, 1080);
        s1.targetFps = 8;
        s1.actualFps = 7.5;
        s1.encodeTimeMs = 9;
        s1.encodeRetryCount = 1;
        s1.ackLatencyMs = -1.0;
        s1.pendingBytes = 0;
        s1.droppedFrames = 0;
        s1.configChangeReason = QString();

        qInfo() << "Unrelated info line";
        QVERIFY(metrics.maybeLog(s1, 1000));

        StreamMetricsSnapshot s2 = s1;
        s2.ackLatencyMs = 42.5;
        s2.pendingBytes = 1024;
        s2.droppedFrames = 2;
        s2.configChangeReason = QStringLiteral("bw_drop");
        QVERIFY(metrics.maybeLog(s2, 2000));

        QCOMPARE(collector.count(), 2);
        const auto entries = collector.entries();
        QVERIFY(entries[0].all15FieldsPresent);
        QVERIFY(entries[0].allFieldsValid);
        QCOMPARE(entries[0].ackLatencyMs, -1.0);
        QCOMPARE(entries[0].configChangeReason, QStringLiteral("steady"));
        QCOMPARE(entries[0].resolution, QSize(1920, 1080));

        QVERIFY(entries[1].all15FieldsPresent);
        QVERIFY(entries[1].allFieldsValid);
        QCOMPARE(entries[1].estimatedBandwidthBps, 250000.0);
        QCOMPARE(entries[1].observedThroughputBps, 180000.0);
        QCOMPARE(entries[1].frameByteBudget, qsizetype(30000));
        QCOMPARE(entries[1].actualFrameBytes, qsizetype(19500));
        QCOMPARE(entries[1].motionScore, 0.25);
        QCOMPARE(entries[1].jpegQuality, 78);
        QCOMPARE(entries[1].targetFps, 8);
        QCOMPARE(entries[1].actualFps, 7.5);
        QCOMPARE(entries[1].encodeTimeMs, qint64(9));
        QCOMPARE(entries[1].encodeRetryCount, 1);
        QCOMPARE(entries[1].ackLatencyMs, 42.5);
        QCOMPARE(entries[1].pendingBytes, qint64(1024));
        QCOMPARE(entries[1].droppedFrames, qint64(2));
        QCOMPARE(entries[1].configChangeReason, QStringLiteral("bw_drop"));
    }

    // Nested RAII check + restored handler check
    {
        E2eOracle::StreamMetricsLogCollector outer(true);
        {
            E2eOracle::StreamMetricsLogCollector inner(true);
            qDebug().noquote() << "[StreamMetrics] estBwBps=1000.0 obsThroughputBps=900.0 "
                                  "byteBudget=5000 frameBytes=4000 motion=0.000 quality=60 "
                                  "res=1280x720 targetFps=5 actualFps=5.00 encodeMs=4 retries=0 "
                                  "ackRttMs=10.0 pendingBytes=0 dropped=0 reason=\"nested\"";
            QCOMPARE(inner.count(), 1);
            QCOMPARE(outer.count(), 1);
        }
        outer.clear();
        QCOMPARE(outer.count(), 0);
    }
}

void E2eHarnessSelfTest::guiResponsivenessProbeMeasuresLoopDelays()
{
    E2eOracle::GuiResponsivenessProbe probe(10);
    probe.start();
    QTest::qWait(120);
    probe.stop();

    QVERIFY2(probe.tickCount() >= 5, qPrintable(QString::number(probe.tickCount())));
    QVERIFY2(probe.maxDelayMs() < 50, qPrintable(QString::number(probe.maxDelayMs())));

    probe.reset();
    QCOMPARE(probe.tickCount(), 0);

    probe.start();
    QTimer::singleShot(25, this, []() { QTest::qSleep(70); });
    QTest::qWait(140);
    probe.stop();

    QVERIFY2(probe.maxDelayMs() >= 50, qPrintable(QString::number(probe.maxDelayMs())));
    QVERIFY(probe.p95DelayMs() > 0.0);
    QVERIFY(probe.meanDelayMs() > 0.0);
}

void E2eHarnessSelfTest::sfinaeSeamDetectorsCompileAndMatchBranchState()
{
    QCOMPARE(E2eOracle::trySetFrameSource(nullptr, []() { return QImage(); }), false);
    QCOMPARE(E2eOracle::trySetCpuUsageSource(nullptr, []() { return 0.0; }), false);
    QCOMPARE(E2eOracle::hasM3AckTelemetrySignals(nullptr), false);

    StaticRelayEndpointProvider provider(QStringLiteral("127.0.0.1"), 12345);
    ChildSessionController child(&provider);
    ScreenStreamSender sender(&child);

    const bool hasFrameSource = E2eOracle::trySetFrameSource(&sender, []() { return QImage(); });
    const bool hasCpuSource = E2eOracle::trySetCpuUsageSource(&sender, []() { return 12.5; });
    const bool hasAckSignals = E2eOracle::hasM3AckTelemetrySignals(&child);

    QCOMPARE(hasFrameSource, E2eOracle::detail::HasSetFrameSource<ScreenStreamSender>::value);
    QCOMPARE(hasCpuSource, E2eOracle::detail::HasSetCpuUsageSource<ScreenStreamSender>::value);
    QCOMPARE(hasAckSignals, E2eOracle::detail::HasM3AckTelemetrySignals<ChildSessionController>::value);
}

void E2eHarnessSelfTest::relayHandshakeOrderingAndFrameDeliveryWork()
{
    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.connectAndStartSession(QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"),
                                        {},
                                        5000,
                                        false));
    QVERIFY(pair.relay->isSessionAcksEnabled());
    QCOMPARE(pair.relay->protocolViolationCount(), 0);

    QSignalSpy frameReadySpy(pair.admin.get(), &AdminSessionController::frameReady);
    QSignalSpy frameReceivedSpy(pair.admin.get(), &AdminSessionController::frameReceived);

    E2eOracle::MarkerColorStepGenerator marker(QSize(640, 400));
    marker.setStep(77);
    const QImage src = marker.nextFrame();
    const QByteArray jpeg = ScreenEncoder::encodeJpeg(src, 80);
    const QList<QByteArray> packets = ScreenFramePacketizer::packetize(jpeg, 1, pair.child->activeSessionId());
    QVERIFY(!packets.isEmpty());

    QVERIFY(pair.child->canSendScreenFrame());
    QVERIFY(pair.child->sendScreenFrame(1, packets));

    QVERIFY(waitUntil([&]() {
        return frameReceivedSpy.count() == 1 && frameReadySpy.count() == 1 && pair.child->canSendScreenFrame();
    }));

    const QImage decoded = frameReadySpy.first().at(1).value<QImage>();
    QCOMPARE(E2eOracle::MarkerColorStepGenerator::detectMarkerStep(decoded), 77);
    QCOMPARE(pair.relay->protocolViolationCount(), 0);
    QCOMPARE(pair.relay->overBudgetPacketCount(), 0);
    QCOMPARE(pair.relay->maxInFlightScreenFrames(), 1);

    pair.cleanup();
}

void E2eHarnessSelfTest::relayValidatesWireLimitsAndInputPackets()
{
    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.connectAndStartSession(QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"),
                                        {},
                                        5000,
                                        false));
    pair.relay->clearRecordedPackets();

    // Send a multi-chunk frame whose individual chunk is <= 524,288 B but > 262,144 B
    const QByteArray oversizedChunkData(265000, '\x5A');
    const QList<QByteArray> overBudgetPackets =
        ScreenFramePacketizer::packetize(oversizedChunkData, 2, pair.child->activeSessionId());
    QCOMPARE(overBudgetPackets.size(), 1);
    QVERIFY(pair.child->sendScreenFrame(2, overBudgetPackets));

    QVERIFY(waitUntil([&]() { return pair.relay->overBudgetPacketCount() == 1; }));
    QCOMPARE(pair.relay->protocolViolationCount(), 0);

    // Invalid input payload size for KEY_PRESS (must be 4 bytes): Admin writes it, relay catches violation
    QVERIFY(pair.admin->sendInput(Protocol::MessageType::KEY_PRESS, QByteArray(5, '\x01')));
    QVERIFY(waitUntil([&]() { return pair.relay->protocolViolationCount() >= 1; }));

    pair.cleanup();
}

void E2eHarnessSelfTest::relayEnforcesBandwidthRttAndJitter()
{
    E2eHarness::NetworkProfile profile;
    profile.bandwidthBps = 40000.0; // 40 KB/s -> ~20 KB takes ~500 ms serialization
    profile.baseRttMs = 120;        // ~60 ms one-way
    profile.jitterMs = 15;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.connectAndStartSession(QStringLiteral("cccccccc-cccc-4ccc-8ccc-cccccccccccc"),
                                        profile,
                                        5000,
                                        false));

    QSignalSpy frameReceivedSpy(pair.admin.get(), &AdminSessionController::frameReceived);

    // Construct a ~20 KB dummy JPEG-like payload
    E2eOracle::StaticTextFrameGenerator gen(QSize(640, 480), false);
    QByteArray jpeg = ScreenEncoder::encodeJpeg(gen.nextFrame(), 70);
    if (jpeg.size() < 20000)
        jpeg.append(QByteArray(20000 - jpeg.size(), '\x00'));

    const QList<QByteArray> packets = ScreenFramePacketizer::packetize(jpeg, 10, pair.child->activeSessionId());

    QElapsedTimer timer;
    timer.start();
    QVERIFY(pair.child->sendScreenFrame(10, packets));
    QVERIFY(waitUntil([&]() { return frameReceivedSpy.count() == 1; }, 4000));
    const qint64 oneWayElapsed = timer.elapsed();

    // Expect at least txMs (~500ms) + baseRtt/2 (60ms) - timer tolerance
    QVERIFY2(oneWayElapsed >= 450, qPrintable(QStringLiteral("oneWayElapsed=%1 ms").arg(oneWayElapsed)));

    // Wait for ACK to complete round-trip
    QVERIFY(waitUntil([&]() { return pair.child->canSendScreenFrame(); }, 4000));
    const qint64 rttElapsed = timer.elapsed();
    QVERIFY2(rttElapsed >= oneWayElapsed + 40,
             qPrintable(QStringLiteral("rttElapsed=%1 oneWay=%2").arg(rttElapsed).arg(oneWayElapsed)));

    pair.cleanup();
}

void E2eHarnessSelfTest::relayEnforcesChunkDropAndLossPercent()
{
    // 1) Deterministic chunkDropIndices
    {
        E2eHarness::NetworkProfile profile;
        profile.chunkDropIndices.insert(0);

        E2eHarness::E2eSessionPair pair;
        QVERIFY(pair.connectAndStartSession(QStringLiteral("dddddddd-dddd-4ddd-8ddd-dddddddddddd"),
                                            profile,
                                            5000,
                                            false));
        QSignalSpy frameReceivedSpy(pair.admin.get(), &AdminSessionController::frameReceived);

        E2eOracle::StaticTextFrameGenerator gen(QSize(320, 240), false);
        const QByteArray jpeg = ScreenEncoder::encodeJpeg(gen.nextFrame(), 70);
        const auto packets = ScreenFramePacketizer::packetize(jpeg, 1, pair.child->activeSessionId());
        QVERIFY(pair.child->sendScreenFrame(1, packets));

        QVERIFY(waitUntil([&]() { return pair.relay->droppedPacketCount() == 1; }));
        QTest::qWait(60);
        QCOMPARE(frameReceivedSpy.count(), 0);
        QCOMPARE(pair.relay->maxInFlightScreenFrames(), 0);
        QVERIFY(!pair.child->canSendScreenFrame()); // ACK never arrives because frame wasn't completed
        pair.cleanup();
    }

    // 2) 100% packet loss
    {
        E2eHarness::NetworkProfile profile;
        profile.lossPercent = 100.0;

        E2eHarness::E2eSessionPair pair;
        QVERIFY(pair.connectAndStartSession(QStringLiteral("eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee"),
                                            profile,
                                            5000,
                                            false));
        QSignalSpy frameReceivedSpy(pair.admin.get(), &AdminSessionController::frameReceived);

        E2eOracle::StaticTextFrameGenerator gen(QSize(320, 240), false);
        const QByteArray jpeg = ScreenEncoder::encodeJpeg(gen.nextFrame(), 70);
        for (quint32 fid = 1; fid <= 3; ++fid) {
            const auto packets = ScreenFramePacketizer::packetize(jpeg, fid, pair.child->activeSessionId());
            QVERIFY(pair.child->sendScreenPacket(packets.first()) > 0);
        }

        QVERIFY(waitUntil([&]() { return pair.relay->droppedPacketCount() == 3; }));
        QTest::qWait(60);
        QCOMPARE(frameReceivedSpy.count(), 0);
        QCOMPARE(pair.relay->maxInFlightScreenFrames(), 0);
        pair.cleanup();
    }
}

void E2eHarnessSelfTest::relayEnforcesAckDelayDropAcksAndUnwritableReservation()
{
    // 1) ackDelayMs delays ACK without blocking input packets
    {
        E2eHarness::NetworkProfile profile;
        profile.ackDelayMs = 250;

        E2eHarness::E2eSessionPair pair;
        QVERIFY(pair.connectAndStartSession(QStringLiteral("ffffffff-ffff-4fff-8fff-ffffffffffff"),
                                            profile,
                                            5000,
                                            false));

        QSignalSpy frameReceivedSpy(pair.admin.get(), &AdminSessionController::frameReceived);
        QSignalSpy inputSpy(pair.child.get(), &ChildSessionController::inputReceived);

        E2eOracle::StaticTextFrameGenerator gen(QSize(320, 240), false);
        const QByteArray jpeg = ScreenEncoder::encodeJpeg(gen.nextFrame(), 70);
        const auto packets = ScreenFramePacketizer::packetize(jpeg, 1, pair.child->activeSessionId());

        QElapsedTimer timer;
        timer.start();
        QVERIFY(pair.child->sendScreenFrame(1, packets));
        QVERIFY(waitUntil([&]() { return frameReceivedSpy.count() == 1; }));

        // Input sent after ACK is queued should arrive much faster than 250 ms ackDelayMs
        QVERIFY(pair.admin->sendInput(Protocol::MessageType::KEY_PRESS, RemoteInputPacket::key(Qt::Key_A)));
        QVERIFY(waitUntil([&]() { return inputSpy.count() == 1; }));
        QVERIFY(!pair.child->canSendScreenFrame());

        QVERIFY(waitUntil([&]() { return pair.child->canSendScreenFrame(); }, 2000));
        QVERIFY2(timer.elapsed() >= 220, qPrintable(QString::number(timer.elapsed())));
        pair.cleanup();
    }

    // 2) dropAcks + simulateUnwritableAdmin 1-packet reservation
    {
        E2eHarness::NetworkProfile profile;
        profile.dropAcks = true;
        profile.simulateUnwritableAdmin = true;

        E2eHarness::E2eSessionPair pair;
        QVERIFY(pair.connectAndStartSession(QStringLiteral("11111111-1111-4111-8111-111111111111"),
                                            profile,
                                            5000,
                                            false));

        QSignalSpy frameReceivedSpy(pair.admin.get(), &AdminSessionController::frameReceived);

        E2eOracle::StaticTextFrameGenerator gen(QSize(320, 240), false);
        const QByteArray jpeg = ScreenEncoder::encodeJpeg(gen.nextFrame(), 70);

        // Oversized packet while simulateUnwritableAdmin=true is dropped without consuming reservation or inflating in-flight count
        const QByteArray oversized(265000, '\x5A');
        const auto overPkts = ScreenFramePacketizer::packetize(oversized, 1, pair.child->activeSessionId());
        QCOMPARE(pair.child->sendScreenPacket(overPkts.first()), overPkts.first().size());
        QVERIFY(waitUntil([&]() {
            return pair.relay->droppedPacketCount() == 1 && pair.relay->overBudgetPacketCount() == 1;
        }));
        QCOMPARE(pair.relay->maxInFlightScreenFrames(), 0);

        // First valid packet <= 262,144 B is reserved and forwarded
        const auto p1 = ScreenFramePacketizer::packetize(jpeg, 2, pair.child->activeSessionId());
        QCOMPARE(pair.child->sendScreenPacket(p1.first()), p1.first().size());
        QVERIFY(waitUntil([&]() { return frameReceivedSpy.count() == 1; }));
        QCOMPARE(pair.relay->droppedPacketCount(), 2); // Admin's ACK was dropped by dropAcks
        QCOMPARE(pair.relay->maxInFlightScreenFrames(), 1);

        // Second valid packet while simulateUnwritableAdmin=true must be dropped without inflating maxInFlightScreenFrames
        const auto p2 = ScreenFramePacketizer::packetize(jpeg, 3, pair.child->activeSessionId());
        QCOMPARE(pair.child->sendScreenPacket(p2.first()), p2.first().size());
        QVERIFY(waitUntil([&]() { return pair.relay->droppedPacketCount() == 3; }));
        QTest::qWait(40);
        QCOMPARE(frameReceivedSpy.count(), 1);
        QCOMPARE(pair.relay->maxInFlightScreenFrames(), 1);

        // Clearing simulateUnwritableAdmin allows forwarding again
        profile.simulateUnwritableAdmin = false;
        profile.dropAcks = false;
        pair.relay->setNetworkProfile(profile);

        const auto p3 = ScreenFramePacketizer::packetize(jpeg, 4, pair.child->activeSessionId());
        QCOMPARE(pair.child->sendScreenPacket(p3.first()), p3.first().size());
        QVERIFY(waitUntil([&]() { return frameReceivedSpy.count() == 2; }));

        pair.cleanup();
    }
}

void E2eHarnessSelfTest::relayEnforcesReadBufferBackpressureAndScheduledProfileChange()
{
    // 1) readBufferLimitBytes pauses draining when queue >= limit and recovers promptly on profile change
    {
        E2eHarness::NetworkProfile profile;
        profile.bandwidthBps = 2000.0;
        profile.readBufferLimitBytes = 4096;

        E2eHarness::E2eSessionPair pair;
        QVERIFY(pair.connectAndStartSession(QStringLiteral("22222222-2222-4222-8222-222222222222"),
                                            profile,
                                            5000,
                                            false));
        pair.relay->clearRecordedPackets();
        QSignalSpy frameReceivedSpy(pair.admin.get(), &AdminSessionController::frameReceived);

        // Send 3 raw 8 KB SCREEN_FRAME packets; packet #1 fills the 4096 B queue limit,
        // causing the relay to pause reading packets #2 and #3 until the queue drains.
        const QByteArray payload8k(8000, '\x42');
        for (quint32 fid = 1; fid <= 3; ++fid) {
            const auto pkts = ScreenFramePacketizer::packetize(payload8k, fid, pair.child->activeSessionId());
            QVERIFY(pair.child->sendScreenPacket(pkts.first()) > 0);
        }

        QVERIFY(waitUntil([&]() {
            int screenCount = 0;
            for (const auto &rec : pair.relay->recordedPackets()) {
                if (rec.header.type == Protocol::MessageType::SCREEN_FRAME)
                    ++screenCount;
            }
            return screenCount >= 1;
        }));
        QTest::qWait(80);
        int pausedScreenCount = 0;
        for (const auto &rec : pair.relay->recordedPackets()) {
            if (rec.header.type == Protocol::MessageType::SCREEN_FRAME)
                ++pausedScreenCount;
        }
        QCOMPARE(pausedScreenCount, 1);
        QCOMPARE(frameReceivedSpy.count(), 0);

        // Schedule profile lift and verify all 3 frames are actually delivered to Admin and Child drains
        E2eHarness::NetworkProfile fastProfile;
        fastProfile.bandwidthBps = 0.0;
        fastProfile.readBufferLimitBytes = 0;
        pair.relay->scheduleProfileChange(30, fastProfile);

        QVERIFY(waitUntil([&]() {
            return frameReceivedSpy.count() == 3 && pair.child->pendingBytes() == 0;
        }, 1500));

        pair.cleanup();
    }

    // 2) Large backlog (10 x 64 KB) under severe bottleneck lifts promptly without inheriting stale 96s timestamp
    {
        E2eHarness::NetworkProfile slowProfile;
        slowProfile.bandwidthBps = 4000.0;
        slowProfile.readBufferLimitBytes = 4096;
        slowProfile.ackCapabilityEnabled = false;

        E2eHarness::E2eSessionPair pair;
        QVERIFY(pair.connectAndStartSession(QStringLiteral("44444444-4444-4444-8444-444444444444"),
                                            slowProfile,
                                            5000,
                                            false));
        QSignalSpy frameReceivedSpy(pair.admin.get(), &AdminSessionController::frameReceived);

        const QByteArray payload64k(64000, '\x33');
        constexpr int packetCount = 10;
        for (quint32 fid = 1; fid <= packetCount; ++fid) {
            const auto pkts = ScreenFramePacketizer::packetize(payload64k, fid, pair.child->activeSessionId());
            QCOMPARE(pkts.size(), 1);
            QVERIFY(pair.child->sendScreenPacket(pkts.first()) > 0);
        }

        QVERIFY(waitUntil([&]() {
            int recorded = 0;
            for (const auto &rec : pair.relay->recordedPackets()) {
                if (rec.header.type == Protocol::MessageType::SCREEN_FRAME)
                    ++recorded;
            }
            return recorded >= 1;
        }));
        QCOMPARE(frameReceivedSpy.count(), 0);

        E2eHarness::NetworkProfile fastProfile;
        fastProfile.bandwidthBps = 0.0;
        fastProfile.readBufferLimitBytes = 0;
        fastProfile.ackCapabilityEnabled = false;
        pair.relay->scheduleProfileChange(30, fastProfile);

        QVERIFY(waitUntil([&]() {
            return frameReceivedSpy.count() == packetCount && pair.child->pendingBytes() == 0;
        }, 2000));
        QCOMPARE(pair.relay->protocolViolationCount(), 0);

        pair.cleanup();
    }

    // 3) Finite bandwidth increase + RTT/ackDelay decrease re-paces queued frames and delayed ACKs
    {
        E2eHarness::NetworkProfile slowProfile;
        slowProfile.bandwidthBps = 2000.0;
        slowProfile.baseRttMs = 1000;
        slowProfile.ackDelayMs = 2000;

        E2eHarness::E2eSessionPair pair;
        QVERIFY(pair.connectAndStartSession(QStringLiteral("55555555-5555-4555-8555-555555555555"),
                                            slowProfile,
                                            5000,
                                            false));
        QSignalSpy frameReceivedSpy(pair.admin.get(), &AdminSessionController::frameReceived);

        const QByteArray payload8k(8000, '\x55');
        const auto pkts = ScreenFramePacketizer::packetize(payload8k, 1, pair.child->activeSessionId());
        QVERIFY(pair.child->sendScreenFrame(1, pkts));
        QTest::qWait(40);
        QCOMPARE(frameReceivedSpy.count(), 0);

        E2eHarness::NetworkProfile upgradedProfile;
        upgradedProfile.bandwidthBps = 1000000.0;
        upgradedProfile.baseRttMs = 10;
        upgradedProfile.ackDelayMs = 0;
        pair.relay->setNetworkProfile(upgradedProfile);

        QVERIFY(waitUntil([&]() {
            return frameReceivedSpy.count() == 1 && pair.child->canSendScreenFrame();
        }, 1000));

        pair.cleanup();
    }
}

void E2eHarnessSelfTest::relayEnforcesSubMillisecondBandwidthShaping()
{
    E2eHarness::NetworkProfile profile;
    profile.bandwidthBps = 1250000.0; // 1.25 MB/s -> 500 B wire packet = 0.4 ms/pkt -> 200 pkts = 80 ms
    profile.baseRttMs = 0;
    profile.jitterMs = 0;
    profile.ackCapabilityEnabled = false;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.connectAndStartSession(QStringLiteral("66666666-6666-4666-8666-666666666666"),
                                        profile,
                                        5000,
                                        false));
    QSignalSpy frameReceivedSpy(pair.admin.get(), &AdminSessionController::frameReceived);

    // 460 B payload + 16 B metadata + 24 B header = 500 B wire packet
    const QByteArray payload460(460, '\x2A');
    constexpr int count = 200;

    QElapsedTimer timer;
    timer.start();
    for (quint32 fid = 1; fid <= count; ++fid) {
        const auto pkts = ScreenFramePacketizer::packetize(payload460, fid, pair.child->activeSessionId());
        QCOMPARE(pkts.size(), 1);
        QCOMPARE(pkts.first().size(), 500);
        QVERIFY(pair.child->sendScreenPacket(pkts.first()) == 500);
    }

    QVERIFY(waitUntil([&]() { return frameReceivedSpy.count() == count; }, 2000));
    const qint64 elapsed = timer.elapsed();

    QVERIFY2(elapsed >= 60, qPrintable(QStringLiteral("Sub-ms shaping collapsed: elapsed=%1 ms").arg(elapsed)));
    QVERIFY2(elapsed < 1000, qPrintable(QStringLiteral("Unexpectedly slow: elapsed=%1 ms").arg(elapsed)));
    QCOMPARE(pair.relay->protocolViolationCount(), 0);

    pair.cleanup();
}

void E2eHarnessSelfTest::relayIgnoresStaleSessionPacketsAfterDisconnect()
{
    // 1) Stale-session SCREEN_FRAME arriving after DISCONNECT must not trigger false protocol violations
    {
        E2eHarness::NetworkProfile profile;
        profile.bandwidthBps = 2000.0;
        profile.readBufferLimitBytes = 4096;
        profile.ackCapabilityEnabled = false;

        E2eHarness::E2eSessionPair pair;
        QVERIFY(pair.connectAndStartSession(QStringLiteral("77777777-7777-4777-8777-777777777777"),
                                            profile,
                                            5000,
                                            false));
        const quint64 sid1 = pair.child->activeSessionId();
        QVERIFY(sid1 != 0);

        const QByteArray payload8k(8000, '\x11');
        const auto p1 = ScreenFramePacketizer::packetize(payload8k, 1, sid1);
        const auto p2 = ScreenFramePacketizer::packetize(payload8k, 2, sid1);
        QVERIFY(pair.child->sendScreenPacket(p1.first()) > 0);
        QVERIFY(waitUntil([&]() {
            int count = 0;
            for (const auto &rec : pair.relay->recordedPackets()) {
                if (rec.header.type == Protocol::MessageType::SCREEN_FRAME)
                    ++count;
            }
            return count == 1;
        }));

        // p2 stays queued in the child socket buffer while readBufferLimitBytes pauses draining
        QVERIFY(pair.child->sendScreenPacket(p2.first()) > 0);

        // Admin disconnects session 1, clearing relay m_activeSessionId -> 0
        pair.admin->disconnectSession();
        QVERIFY(waitUntil([&]() {
            return pair.relay->activeSessionId() == 0 && pair.child->activeSessionId() == 0;
        }));

        // Lift readBufferLimitBytes so the relay drains in-flight p2 (with stale sid1) after DISCONNECT
        E2eHarness::NetworkProfile openProfile;
        openProfile.ackCapabilityEnabled = false;
        pair.relay->setNetworkProfile(openProfile);
        QTest::qWait(50);
        QCOMPARE(pair.relay->protocolViolationCount(), 0);

        pair.cleanup();
    }

    // 2) Stale-session SCREEN_FRAME_ACK after DISCONNECT is ignored, while sessionId==0, unknown, or wrong-direction still violates
    {
        E2eHarness::SimulatedRdtpRelay relay;
        QVERIFY(relay.start(0));
        QTcpSocket childSock;
        QTcpSocket adminSock;
        childSock.connectToHost(QStringLiteral("127.0.0.1"), relay.port());
        adminSock.connectToHost(QStringLiteral("127.0.0.1"), relay.port());
        QVERIFY(childSock.waitForConnected(2000));
        QVERIFY(adminSock.waitForConnected(2000));

        const QByteArray childAuth = Protocol::relayAuthPayload(QStringLiteral("tok"));
        const QString targetUuid = QStringLiteral("99999999-9999-4999-8999-999999999999");
        const QByteArray adminAuth = Protocol::relayAuthPayload(QStringLiteral("tok"), targetUuid);

        auto makeHdr = [](Protocol::MessageType type, quint64 sid, quint32 seq, quint16 flags, quint32 len) {
            Protocol::ProtocolHeader h(type);
            h.sessionId = sid;
            h.sequenceNumber = seq;
            h.flags = flags;
            h.payloadLength = len;
            return Protocol::ProtocolSerializer::serializeHeader(h);
        };

        childSock.write(makeHdr(Protocol::MessageType::REGISTER_HOST, 0, 0, 0, childAuth.size()) + childAuth);
        QVERIFY(waitUntil([&]() { return childSock.bytesAvailable() >= Protocol::HEADER_SIZE + 1; }));
        childSock.readAll();

        adminSock.write(makeHdr(Protocol::MessageType::CONNECT_REQUEST, 0, 0, 0, adminAuth.size()) + adminAuth);
        QVERIFY(waitUntil([&]() { return childSock.bytesAvailable() >= Protocol::HEADER_SIZE; }));
        const auto reqMsgs = Protocol::RdtpStreamParser().feed(childSock.readAll()).messages;
        QCOMPARE(reqMsgs.size(), 1);
        const quint64 relaySid = reqMsgs.first().header.sessionId;
        QVERIFY(relaySid > 0);

        childSock.write(makeHdr(Protocol::MessageType::SESSION_ACCEPT, relaySid, 0, 0, 0));
        QVERIFY(waitUntil([&]() { return relay.activeSessionId() == relaySid; }));
        adminSock.readAll();

        // Disconnect session
        adminSock.write(makeHdr(Protocol::MessageType::DISCONNECT, relaySid, 0, 0, 0));
        QVERIFY(waitUntil([&]() { return relay.activeSessionId() == 0; }));

        // Stale SCREEN_FRAME from child and stale SCREEN_FRAME_ACK from admin for relaySid -> 0 violations
        const auto staleFrames = ScreenFramePacketizer::packetize(QByteArray(100, '\x11'), 1, relaySid);
        childSock.write(staleFrames.first());
        adminSock.write(makeHdr(Protocol::MessageType::SCREEN_FRAME_ACK, relaySid, 1, 0, 0));
        QTest::qWait(40);
        QCOMPARE(relay.protocolViolationCount(), 0);

        // Unknown sessionId (9999), sessionId == 0, and wrong-direction senders must STILL record violations
        const auto zeroSidFrames = ScreenFramePacketizer::packetize(QByteArray(100, '\x11'), 2, 0);
        const auto bogusSidFrames = ScreenFramePacketizer::packetize(QByteArray(100, '\x11'), 3, 9999);
        const auto wrongDirFrames = ScreenFramePacketizer::packetize(QByteArray(100, '\x11'), 4, relaySid);
        childSock.write(zeroSidFrames.first());
        childSock.write(bogusSidFrames.first());
        adminSock.write(makeHdr(Protocol::MessageType::SCREEN_FRAME_ACK, 9999, 1, 0, 0));
        adminSock.write(wrongDirFrames.first());
        childSock.write(makeHdr(Protocol::MessageType::SCREEN_FRAME_ACK, relaySid, 1, 0, 0));
        QVERIFY(waitUntil([&]() { return relay.protocolViolationCount() == 5; }));

        childSock.close();
        adminSock.close();
        relay.stop();
    }
}

void E2eHarnessSelfTest::relayForwardsInputPacketsAcrossActiveSession()
{
    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.connectAndStartSession(QStringLiteral("33333333-3333-4333-8333-333333333333"),
                                        {},
                                        5000,
                                        true));
    QVERIFY(pair.sender != nullptr);

    QList<QPair<Protocol::ProtocolHeader, QByteArray>> receivedInputs;
    connect(pair.child.get(), &ChildSessionController::inputReceived, this,
            [&receivedInputs](const Protocol::ProtocolHeader &header, const QByteArray &payload) {
                receivedInputs.append({header, payload});
            });

    const QByteArray movePayload = RemoteInputPacket::withFrame(
        RemoteInputPacket::position(320, 180), QSize(1280, 720));
    const QByteArray wheelPayload = RemoteInputPacket::withFrame(
        RemoteInputPacket::wheel(320, 180, -120), QSize(1280, 720));
    const QByteArray keyPayload = RemoteInputPacket::key(Qt::Key_Return);

    QVERIFY(pair.admin->sendInput(Protocol::MessageType::MOUSE_MOVE, movePayload));
    QVERIFY(pair.admin->sendInput(Protocol::MessageType::MOUSE_WHEEL, wheelPayload));
    QVERIFY(pair.admin->sendInput(Protocol::MessageType::KEY_PRESS, keyPayload));

    QVERIFY(waitUntil([&]() { return receivedInputs.size() == 3; }));
    QCOMPARE(pair.relay->protocolViolationCount(), 0);

    QCOMPARE(receivedInputs.at(0).first.type, Protocol::MessageType::MOUSE_MOVE);
    QCOMPARE(receivedInputs.at(0).second, movePayload);

    QCOMPARE(receivedInputs.at(1).first.type, Protocol::MessageType::MOUSE_WHEEL);
    QCOMPARE(receivedInputs.at(1).second, wheelPayload);

    QCOMPARE(receivedInputs.at(2).first.type, Protocol::MessageType::KEY_PRESS);
    QCOMPARE(receivedInputs.at(2).second, keyPayload);

    pair.cleanup();
}

QTEST_MAIN(E2eHarnessSelfTest)
#include "E2eHarnessSelfTest.moc"
