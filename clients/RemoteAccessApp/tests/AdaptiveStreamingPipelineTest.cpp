#include <QtTest>
#include <QSignalSpy>

#include "Network/Protocol/ProtocolConstants.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include "Network/Protocol/ScreenFramePacketizer.h"
#include "Network/Relay/AdminSessionController.h"
#include "Network/Relay/ChildSessionController.h"
#include "Streaming/ScreenStreamSender.h"
#include "e2e/E2eFrameOracle.h"
#include "e2e/E2eRelayHarness.h"

class AdaptiveStreamingPipelineTest final : public QObject
{
    Q_OBJECT

private slots:
    void m3SeamDetectorsConfirmPublicInterfaces();
    void childSessionEmitsAckRttAndSoftStallTelemetry();
    void senderStreamsStaticTextWithHighResolutionAndSharpness();
    void senderAdaptsWeightsAndFpsBetweenStaticTextAndHighMotionVideo();
    void senderBacksOffOnBandwidthDropAndProbesOnRecovery();
    void senderThrottlesOnCpuOverloadAndRecovers();
    void senderEnforcesSingleInFlightAndProtocolPayloadLimitOnNoise();
    void senderKeepsGuiThreadResponsiveAndPreservesControlMessages();
    void senderHandlesSessionLifecycleReconnectAndSafeDestruction();
    void senderEmitsRateLimited15FieldStreamMetricsLogs();
};

void AdaptiveStreamingPipelineTest::m3SeamDetectorsConfirmPublicInterfaces()
{
    ChildSessionController child(nullptr);
    ScreenStreamSender sender(&child);

    QVERIFY(E2eOracle::hasM3AckTelemetrySignals(&child));
    QVERIFY(E2eOracle::trySetFrameSource(&sender, []() {
        return QImage(320, 180, QImage::Format_RGB32);
    }));
    QVERIFY(E2eOracle::trySetCpuUsageSource(&sender, []() {
        return 15.0;
    }));
}

void AdaptiveStreamingPipelineTest::childSessionEmitsAckRttAndSoftStallTelemetry()
{
    E2eHarness::NetworkProfile profile;
    profile.bandwidthBps = 2000000.0;
    profile.baseRttMs = 40;
    profile.ackCapabilityEnabled = true;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.connectAndStartSession(
        QStringLiteral("11111111-1111-4111-8111-111111111111"), profile, 5000, false));
    QVERIFY(pair.child->isScreenFrameAcksEnabled());

    QSignalSpy ackSpy(pair.child.get(), &ChildSessionController::screenFrameAcknowledged);
    QSignalSpy stallSpy(pair.child.get(), &ChildSessionController::screenFrameAckStalled);

    QImage img(320, 180, QImage::Format_RGB32);
    img.fill(Qt::darkCyan);
    const QByteArray jpeg = ScreenEncoder::encodeJpeg(img, 60);
    const QList<QByteArray> pkts1 =
        ScreenFramePacketizer::packetize(jpeg, 1, pair.child->activeSessionId());
    QVERIFY(pair.child->sendScreenFrame(1, pkts1));
    QVERIFY(!pair.child->canSendScreenFrame());

    QTRY_COMPARE_WITH_TIMEOUT(ackSpy.count(), 1, 2000);
    QVERIFY(pair.child->canSendScreenFrame());
    const QList<QVariant> ackArgs = ackSpy.takeFirst();
    QCOMPARE(ackArgs.at(0).toUInt(), 1u);
    QVERIFY(ackArgs.at(1).toLongLong() >= 20);
    QVERIFY(ackArgs.at(2).toLongLong() > 40);
    QCOMPARE(stallSpy.count(), 0);

    // Test soft ACK stall signal when ACKs are delayed
    pair.child->setAckStallIntervalMs(150);
    E2eHarness::NetworkProfile stalledProfile = profile;
    stalledProfile.ackDelayMs = 450;
    pair.relay->setNetworkProfile(stalledProfile);

    const QList<QByteArray> pkts2 =
        ScreenFramePacketizer::packetize(jpeg, 2, pair.child->activeSessionId());
    QVERIFY(pair.child->sendScreenFrame(2, pkts2));
    QTRY_VERIFY_WITH_TIMEOUT(stallSpy.count() >= 1, 1500);
    const QList<QVariant> stallArgs = stallSpy.first();
    QCOMPARE(stallArgs.at(0).toUInt(), 2u);
    QVERIFY(stallArgs.at(1).toLongLong() >= 100);

    // ACK eventually arrives and releases the in-flight slot without killing the session
    QTRY_COMPARE_WITH_TIMEOUT(ackSpy.count(), 1, 2000);
    QVERIFY(pair.child->canSendScreenFrame());
    QVERIFY(pair.child->activeSessionId() != 0);
}

void AdaptiveStreamingPipelineTest::senderStreamsStaticTextWithHighResolutionAndSharpness()
{
    E2eHarness::NetworkProfile profile;
    profile.bandwidthBps = 4000000.0; // 4 MB/s LAN/fast link
    profile.baseRttMs = 10;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.initialize(profile));

    E2eOracle::StaticTextFrameGenerator textGen(QSize(1280, 720), true);
    pair.sender->setFrameSource([&textGen]() { return textGen.nextFrame(); });

    BandwidthEstimator::Config bwCfg = pair.sender->bandwidthEstimatorConfig();
    bwCfg.initialBandwidthBps = 2000000.0;
    pair.sender->setBandwidthEstimatorConfig(bwCfg);

    QList<QImage> decodedFrames;
    QList<QByteArray> rawJpegs;
    connect(pair.admin.get(), &AdminSessionController::frameReady,
            this, [&decodedFrames](quint64, const QImage &img) { decodedFrames.append(img); });
    connect(pair.admin.get(), &AdminSessionController::frameReceived,
            this, [&rawJpegs](quint32, const QByteArray &data) { rawJpegs.append(data); });

    QVERIFY(pair.connectAndStartSession(
        QStringLiteral("22222222-2222-4222-8222-222222222222"), profile, 5000, true));

    QTRY_VERIFY_WITH_TIMEOUT(decodedFrames.size() >= 3, 3000);
    QVERIFY(!rawJpegs.isEmpty());

    const QImage lastDecoded = decodedFrames.last();
    QCOMPARE(lastDecoded.size(), QSize(1280, 720)); // r = 1.0 preserved for text readability

    const int estQ = E2eOracle::estimateJpegQualityFromDqt(rawJpegs.last());
    QVERIFY2(estQ >= 70, qPrintable(QStringLiteral("Expected quality >= 70 for static text, got %1").arg(estQ)));
    QCOMPARE(pair.relay->protocolViolationCount(), 0);
    QCOMPARE(pair.relay->overBudgetPacketCount(), 0);
    QCOMPARE(pair.relay->maxInFlightScreenFrames(), 1);
}

void AdaptiveStreamingPipelineTest::senderAdaptsWeightsAndFpsBetweenStaticTextAndHighMotionVideo()
{
    E2eHarness::NetworkProfile profile;
    profile.bandwidthBps = 3000000.0;
    profile.baseRttMs = 10;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.initialize(profile));

    BandwidthEstimator::Config bwCfg = pair.sender->bandwidthEstimatorConfig();
    bwCfg.initialBandwidthBps = 2000000.0;
    pair.sender->setBandwidthEstimatorConfig(bwCfg);

    AdaptiveStreamController::Config ctrlCfg = pair.sender->controllerConfig();
    ctrlCfg.slowLoopIntervalMs = 500;
    ctrlCfg.minEventSlowLoopIntervalMs = 100;
    pair.sender->setControllerConfig(ctrlCfg);

    E2eOracle::StaticTextFrameGenerator staticGen(QSize(960, 540), false);
    E2eOracle::HighMotionVideoFrameGenerator videoGen(QSize(960, 540));
    bool useVideo = false;
    pair.sender->setFrameSource([&]() {
        return useVideo ? videoGen.nextFrame() : staticGen.nextFrame();
    });

    int receivedCount = 0;
    connect(pair.admin.get(), &AdminSessionController::frameReceived,
            this, [&receivedCount](quint32, const QByteArray &) { ++receivedCount; });

    QVERIFY(pair.connectAndStartSession(
        QStringLiteral("33333333-3333-4333-8333-333333333333"), profile, 5000, true));

    QTRY_VERIFY_WITH_TIMEOUT(receivedCount >= 3, 2500);
    const double staticMotion = pair.sender->smoothedMotionScore();
    QVERIFY2(staticMotion < 0.15, qPrintable(QString::number(staticMotion)));
    QCOMPARE(pair.sender->activeStreamConfig().scale, 1.0);

    // Switch to continuous high-motion video
    useVideo = true;
    const int beforeVideoCount = receivedCount;
    QTRY_VERIFY_WITH_TIMEOUT(receivedCount - beforeVideoCount >= 6, 2500);
    const double videoMotion = pair.sender->smoothedMotionScore();
    QVERIFY2(videoMotion > 0.25, qPrintable(QString::number(videoMotion)));
    QVERIFY(pair.sender->activeStreamConfig().fps >= 8);
}

void AdaptiveStreamingPipelineTest::senderBacksOffOnBandwidthDropAndProbesOnRecovery()
{
    E2eHarness::NetworkProfile fastProfile;
    fastProfile.bandwidthBps = 2500000.0;
    fastProfile.baseRttMs = 15;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.initialize(fastProfile));

    BandwidthEstimator::Config bwCfg = pair.sender->bandwidthEstimatorConfig();
    bwCfg.initialBandwidthBps = 1500000.0;
    bwCfg.probeIntervalMs = 120;
    bwCfg.probeStableAckCount = 2;
    bwCfg.backoffCooldownMs = 80;
    pair.sender->setBandwidthEstimatorConfig(bwCfg);

    AdaptiveStreamController::Config ctrlCfg = pair.sender->controllerConfig();
    ctrlCfg.slowLoopIntervalMs = 500;
    ctrlCfg.minEventSlowLoopIntervalMs = 80;
    pair.sender->setControllerConfig(ctrlCfg);

    E2eOracle::ScrollingUiFrameGenerator scrollGen(QSize(960, 540), 28);
    pair.sender->setFrameSource([&scrollGen]() { return scrollGen.nextFrame(); });
    pair.child->setAckStallIntervalMs(180);

    int frames = 0;
    connect(pair.admin.get(), &AdminSessionController::frameReceived,
            this, [&frames](quint32, const QByteArray &) { ++frames; });

    QVERIFY(pair.connectAndStartSession(
        QStringLiteral("44444444-4444-4444-8444-444444444444"), fastProfile, 5000, true));

    QTRY_VERIFY_WITH_TIMEOUT(frames >= 4, 2000);
    const double peakBw = pair.sender->estimatedBandwidthBps();
    QVERIFY(peakBw >= 1000000.0);

    // Induce sharp congestion + ACK stall
    E2eHarness::NetworkProfile congestedProfile;
    congestedProfile.bandwidthBps = 120000.0;
    congestedProfile.baseRttMs = 180;
    congestedProfile.ackDelayMs = 250;
    pair.relay->setNetworkProfile(congestedProfile);

    QTRY_VERIFY_WITH_TIMEOUT(pair.sender->estimatedBandwidthBps() < peakBw * 0.75, 2500);
    const double troughBw = pair.sender->estimatedBandwidthBps();

    // Restore high bandwidth and verify upward probing recovery
    pair.relay->setNetworkProfile(fastProfile);
    QTRY_VERIFY_WITH_TIMEOUT(pair.sender->estimatedBandwidthBps() > troughBw * 1.15, 3000);
    QCOMPARE(pair.relay->overBudgetPacketCount(), 0);
}

void AdaptiveStreamingPipelineTest::senderThrottlesOnCpuOverloadAndRecovers()
{
    E2eHarness::NetworkProfile profile;
    profile.bandwidthBps = 4000000.0;
    profile.baseRttMs = 10;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.initialize(profile));

    BandwidthEstimator::Config bwCfg = pair.sender->bandwidthEstimatorConfig();
    bwCfg.initialBandwidthBps = 2500000.0;
    pair.sender->setBandwidthEstimatorConfig(bwCfg);

    AdaptiveStreamController::Config ctrlCfg = pair.sender->controllerConfig();
    ctrlCfg.slowLoopIntervalMs = 500;
    ctrlCfg.minEventSlowLoopIntervalMs = 80;
    ctrlCfg.upgradeStabilityTicks = 2;
    ctrlCfg.upgradeStabilityWindowMs = 300;
    pair.sender->setControllerConfig(ctrlCfg);

    E2eOracle::HighMotionVideoFrameGenerator videoGen(QSize(960, 540));
    pair.sender->setFrameSource([&videoGen]() { return videoGen.nextFrame(); });

    double simulatedCpu = 25.0;
    pair.sender->setCpuUsageSource([&simulatedCpu]() { return simulatedCpu; });

    int frames = 0;
    connect(pair.admin.get(), &AdminSessionController::frameReceived,
            this, [&frames](quint32, const QByteArray &) { ++frames; });

    QVERIFY(pair.connectAndStartSession(
        QStringLiteral("55555555-5555-4555-8555-555555555555"), profile, 5000, true));

    QTRY_VERIFY_WITH_TIMEOUT(frames >= 4, 2000);
    const int normalFps = pair.sender->activeStreamConfig().fps;
    QVERIFY(normalFps >= 8);

    // Spike CPU usage to 94%
    simulatedCpu = 94.0;
    const int countAtOverload = frames;
    QTRY_VERIFY_WITH_TIMEOUT(frames - countAtOverload >= 3, 2000);
    const int throttledFps = pair.sender->activeStreamConfig().fps;
    QVERIFY2(throttledFps < normalFps,
             qPrintable(QStringLiteral("Expected throttledFps (%1) < normalFps (%2)")
                            .arg(throttledFps)
                            .arg(normalFps)));
}

void AdaptiveStreamingPipelineTest::senderEnforcesSingleInFlightAndProtocolPayloadLimitOnNoise()
{
    E2eHarness::NetworkProfile profile;
    profile.bandwidthBps = 10000000.0;
    profile.baseRttMs = 5;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.initialize(profile));

    BandwidthEstimator::Config bwCfg = pair.sender->bandwidthEstimatorConfig();
    bwCfg.initialBandwidthBps = 8000000.0;
    pair.sender->setBandwidthEstimatorConfig(bwCfg);

    E2eOracle::WorstCaseEntropyNoiseGenerator noiseGen(QSize(1920, 1080), 99u);
    pair.sender->setFrameSource([&noiseGen]() { return noiseGen.nextFrame(); });

    int receivedFrames = 0;
    connect(pair.admin.get(), &AdminSessionController::frameReceived,
            this, [&receivedFrames](quint32, const QByteArray &data) {
                QVERIFY(data.size() <= 262104);
                ++receivedFrames;
            });

    QVERIFY(pair.connectAndStartSession(
        QStringLiteral("66666666-6666-4666-8666-666666666666"), profile, 5000, true));

    QTRY_VERIFY_WITH_TIMEOUT(receivedFrames >= 4, 3000);
    QCOMPARE(pair.relay->protocolViolationCount(), 0);
    QCOMPARE(pair.relay->overBudgetPacketCount(), 0);
    QCOMPARE(pair.relay->maxInFlightScreenFrames(), 1);
}

void AdaptiveStreamingPipelineTest::senderKeepsGuiThreadResponsiveAndPreservesControlMessages()
{
    E2eHarness::NetworkProfile profile;
    profile.bandwidthBps = 600000.0;
    profile.baseRttMs = 30;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.initialize(profile));

    E2eOracle::ScrollingUiFrameGenerator scrollGen(QSize(1280, 720), 20);
    pair.sender->setFrameSource([&scrollGen]() { return scrollGen.nextFrame(); });

    QSignalSpy inputSpy(pair.child.get(), &ChildSessionController::inputReceived);
    QSignalSpy capturedSpy(pair.sender.get(), &ScreenStreamSender::frameCaptured);

    E2eOracle::GuiResponsivenessProbe guiProbe(10);
    guiProbe.start();

    QVERIFY(pair.connectAndStartSession(
        QStringLiteral("77777777-7777-4777-8777-777777777777"), profile, 5000, true));

    QTRY_VERIFY_WITH_TIMEOUT(capturedSpy.count() >= 2, 2000);

    // Send burst of mouse & keyboard input packets from Admin while Child is streaming
    for (int i = 0; i < 10; ++i) {
        QVERIFY(pair.admin->sendInput(
            Protocol::MessageType::MOUSE_MOVE,
            RemoteInputPacket::withFrame(RemoteInputPacket::position(100 + i * 5, 200 + i * 3), QSize(1280, 720))));
    }

    QTRY_COMPARE_WITH_TIMEOUT(inputSpy.count(), 10, 2000);
    guiProbe.stop();

    QVERIFY2(guiProbe.maxDelayMs() < 80,
             qPrintable(QStringLiteral("GUI thread stall %1 ms exceeded limit").arg(guiProbe.maxDelayMs())));
    QCOMPARE(pair.relay->protocolViolationCount(), 0);
}

void AdaptiveStreamingPipelineTest::senderHandlesSessionLifecycleReconnectAndSafeDestruction()
{
    E2eHarness::NetworkProfile profile;
    profile.bandwidthBps = 2000000.0;
    profile.baseRttMs = 10;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.initialize(profile));

    E2eOracle::StaticTextFrameGenerator textGen(QSize(1280, 720), true);
    pair.sender->setFrameSource([&textGen]() { return textGen.nextFrame(); });

    int adminFrames = 0;
    connect(pair.admin.get(), &AdminSessionController::frameReceived,
            this, [&adminFrames](quint32, const QByteArray &) { ++adminFrames; });

    QVERIFY(pair.connectAndStartSession(
        QStringLiteral("88888888-8888-4888-8888-888888888888"), profile, 5000, true));
    QTRY_VERIFY_WITH_TIMEOUT(adminFrames >= 2, 2000);

    // End session and verify streaming stops cleanly
    pair.admin->disconnectSession();
    QTRY_COMPARE_WITH_TIMEOUT(pair.child->activeSessionId(), quint64(0), 2000);

    // Destroy sender while session just ended or during active tick without crash
    pair.sender.reset();
    QVERIFY(!pair.sender);
}

void AdaptiveStreamingPipelineTest::senderEmitsRateLimited15FieldStreamMetricsLogs()
{
    E2eOracle::StreamMetricsLogCollector logCollector(false);

    E2eHarness::NetworkProfile profile;
    profile.bandwidthBps = 2500000.0;
    profile.baseRttMs = 15;

    E2eHarness::E2eSessionPair pair;
    QVERIFY(pair.initialize(profile));
    pair.sender->setMetricsLogIntervalMs(250);

    E2eOracle::ScrollingUiFrameGenerator scrollGen(QSize(960, 540), 16);
    pair.sender->setFrameSource([&scrollGen]() { return scrollGen.nextFrame(); });

    QVERIFY(pair.connectAndStartSession(
        QStringLiteral("99999999-9999-4999-8999-999999999999"), profile, 5000, true));

    QTRY_VERIFY_WITH_TIMEOUT(logCollector.count() >= 2, 2500);
    const QList<E2eOracle::ParsedMetricsEntry> entries = logCollector.entries();
    QVERIFY(!entries.isEmpty());
    for (const auto &entry : entries) {
        QVERIFY2(entry.all15FieldsPresent, qPrintable(entry.rawLine));
        QVERIFY2(entry.allFieldsValid, qPrintable(entry.rawLine));
    }
}

QTEST_MAIN(AdaptiveStreamingPipelineTest)
#include "AdaptiveStreamingPipelineTest.moc"
