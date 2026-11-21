#include "ScreenStreamSender.h"
#include "Network/Relay/ChildSessionController.h"
#include "Network/Protocol/ScreenFramePacketizer.h"

#include <QDebug>
#include <QMutexLocker>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <utility>

ScreenStreamSender::ScreenStreamSender(ChildSessionController *session, QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_session(session)
    , m_frameId(1)
    , m_currentSessionId(0)
    , m_sessionEpoch(0)
    , m_bandwidthEstimator(BandwidthEstimator::Config::fromEnvironment())
{
    m_clock.start();
    m_timer->setInterval(DEFAULT_TICK_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &ScreenStreamSender::onTick);
    connect(&m_encoder, &QFutureWatcher<WorkerResult>::finished, this, &ScreenStreamSender::sendEncodedFrame);

    if (m_session) {
        connect(m_session, &ChildSessionController::sessionStarted, this, [this](quint64 id) {
            ++m_sessionEpoch;
            m_currentSessionId = id;
            m_frameId = 1;
            m_lastSentFrame = QImage();
            m_encodingFrame = QImage();
            m_encodingSessionId = 0;
            m_lastSentTimer.invalidate();
            m_nextCaptureDelayMs = 0;
            m_lastAckLatencyMs = -1.0;
            m_lastCpuSampleMs = -1000;
            m_cachedCpuPercent.reset();
            m_telemetrySampler.reset();
            m_motionDetector.reset();
            m_bandwidthEstimator.reset();
            m_controller.reset();
            m_metrics.reset();
            {
                QMutexLocker locker(&m_snapshotMutex);
                m_latestSnapshot = StreamMetricsSnapshot{};
            }
            m_timer->setInterval(DEFAULT_TICK_INTERVAL_MS);
            m_timer->start();
            QMetaObject::invokeMethod(this, &ScreenStreamSender::onTick, Qt::QueuedConnection);
        });

        connect(m_session, &ChildSessionController::sessionEnded, this, [this] {
            ++m_sessionEpoch;
            m_currentSessionId = 0;
            m_lastSentFrame = QImage();
            m_encodingFrame = QImage();
            m_encodingSessionId = 0;
            m_lastSentTimer.invalidate();
            m_nextCaptureDelayMs = 0;
            m_timer->stop();
        });

        connect(m_session, &ChildSessionController::screenFrameAcknowledged,
                this, [this](quint32 frameId, qint64 rttMs, qsizetype /*wireBytes*/) {
            if (m_currentSessionId == 0)
                return;
            const qint64 nowMs = currentNowMs();
            const qint64 pending = m_session ? m_session->pendingBytes() : 0;
            m_bandwidthEstimator.onFrameAcknowledged(frameId, nowMs, pending);
            m_lastAckLatencyMs = static_cast<double>(qMax<qint64>(1, rttMs));

            if (!m_workerBusy && m_session && m_session->canSendScreenFrame()) {
                const qint64 elapsed = m_lastSentTimer.isValid()
                        ? m_lastSentTimer.elapsed()
                        : m_nextCaptureDelayMs;
                if (elapsed >= m_nextCaptureDelayMs) {
                    QMetaObject::invokeMethod(this, &ScreenStreamSender::onTick, Qt::QueuedConnection);
                }
            }
        });

        connect(m_session, &ChildSessionController::screenFrameAckStalled,
                this, [this](quint32 frameId, qint64 elapsedMs) {
            if (m_currentSessionId == 0)
                return;
            const qint64 pending = m_session ? m_session->pendingBytes() : 0;
            m_bandwidthEstimator.onAckTimeout(frameId, elapsedMs, pending);
            m_lastAckLatencyMs = static_cast<double>(qMax<qint64>(1, elapsedMs));
        });
    }
}

ScreenStreamSender::~ScreenStreamSender()
{
    ++m_sessionEpoch;
    m_currentSessionId = 0;
    if (m_timer) {
        m_timer->stop();
    }
    if (m_encoder.isRunning()) {
        m_encoder.waitForFinished();
    }
}

void ScreenStreamSender::setFrameSource(std::function<QImage()> source)
{
    m_frameSource = std::move(source);
}

void ScreenStreamSender::setCpuUsageSource(std::function<double()> source)
{
    m_cpuUsageSource = std::move(source);
}

void ScreenStreamSender::setControllerConfig(const AdaptiveStreamController::Config &config)
{
    m_controller.setConfig(config);
}

AdaptiveStreamController::Config ScreenStreamSender::controllerConfig() const
{
    return m_controller.config();
}

void ScreenStreamSender::setBandwidthEstimatorConfig(const BandwidthEstimator::Config &config)
{
    m_bandwidthEstimator.setConfig(config);
}

BandwidthEstimator::Config ScreenStreamSender::bandwidthEstimatorConfig() const
{
    return m_bandwidthEstimator.config();
}

void ScreenStreamSender::setMotionDetectorConfig(const MotionDetector::Config &config)
{
    m_motionDetector.setConfig(config);
}

MotionDetector::Config ScreenStreamSender::motionDetectorConfig() const
{
    return m_motionDetector.config();
}

void ScreenStreamSender::setMetricsLogIntervalMs(qint64 intervalMs)
{
    m_metrics.setLogIntervalMs(intervalMs);
}

qint64 ScreenStreamSender::metricsLogIntervalMs() const
{
    return m_metrics.logIntervalMs();
}

StreamMetricsSnapshot ScreenStreamSender::latestMetricsSnapshot() const
{
    QMutexLocker locker(&m_snapshotMutex);
    return m_latestSnapshot;
}

StreamConfig ScreenStreamSender::activeStreamConfig() const
{
    return m_controller.activeConfig();
}

double ScreenStreamSender::estimatedBandwidthBps() const
{
    return m_bandwidthEstimator.estimatedBandwidthBps();
}

double ScreenStreamSender::availableBandwidthBps() const
{
    return m_bandwidthEstimator.availableBandwidthBps();
}

double ScreenStreamSender::observedThroughputBps() const
{
    return m_bandwidthEstimator.observedThroughputBps();
}

double ScreenStreamSender::smoothedRttMs() const
{
    return m_bandwidthEstimator.smoothedRttMs();
}

double ScreenStreamSender::smoothedMotionScore() const
{
    return m_motionDetector.smoothedScore();
}

qint64 ScreenStreamSender::droppedFramesCount() const
{
    return m_metrics.droppedFrames();
}

qint64 ScreenStreamSender::currentNowMs() const
{
    return m_clock.isValid() ? m_clock.elapsed() : 0;
}

std::optional<double> ScreenStreamSender::sampleCpuUsage(qint64 nowMs)
{
    if (m_cpuUsageSource) {
        const double val = m_cpuUsageSource();
        if (std::isfinite(val) && val >= 0.0) {
            return std::clamp(val, 0.0, 100.0);
        }
        return std::nullopt;
    }

    if (nowMs - m_lastCpuSampleMs >= 500) {
        m_lastCpuSampleMs = nowMs;
        const TelemetryInfo info = m_telemetrySampler.sample();
        if (info.cpuPercent.has_value() && std::isfinite(*info.cpuPercent)) {
            m_cachedCpuPercent = std::clamp(*info.cpuPercent, 0.0, 100.0);
        }
    }
    return m_cachedCpuPercent;
}

ScreenStreamSender::FrameChange ScreenStreamSender::classifyFrameChange(const QImage &current, const QImage &previous)
{
    if (previous.isNull() || current.size() != previous.size() || current.format() != previous.format()) {
        return FrameChange::Significant;
    }
    const int width = current.width();
    const int height = current.height();
    if (width <= 0 || height <= 0) {
        return FrameChange::None;
    }

    const QImage cur32 = (current.depth() == 32) ? current : current.convertToFormat(QImage::Format_RGB32);
    const QImage prev32 = (previous.depth() == 32) ? previous : previous.convertToFormat(QImage::Format_RGB32);

    const int step = (width >= 640 && height >= 480) ? 2 : 1;
    const int samplesX = (width + step - 1) / step;
    const int samplesY = (height + step - 1) / step;
    const int totalSamples = samplesX * samplesY;
    const int significantThreshold = qMax(16, totalSamples / 3000);
    const int minorNoiseFloor = qMax(2, totalSamples / 100000);

    int changedSamples = 0;
    for (int y = 0; y < height; y += step) {
        const auto *curLine = reinterpret_cast<const QRgb *>(cur32.constScanLine(y));
        const auto *prevLine = reinterpret_cast<const QRgb *>(prev32.constScanLine(y));
        for (int x = 0; x < width; x += step) {
            const QRgb a = curLine[x];
            const QRgb b = prevLine[x];
            if (a == b) continue;
            const int diff = qAbs(qRed(a) - qRed(b))
                           + qAbs(qGreen(a) - qGreen(b))
                           + qAbs(qBlue(a) - qBlue(b));
            if (diff > 18) {
                if (++changedSamples >= significantThreshold) {
                    return FrameChange::Significant;
                }
            }
        }
    }

    if (changedSamples <= minorNoiseFloor) {
        return FrameChange::None;
    }
    return FrameChange::Minor;
}

void ScreenStreamSender::onTick()
{
    if (m_currentSessionId == 0 || !m_session || m_workerBusy || m_encoder.isRunning())
        return;

    const qint64 nowMs = currentNowMs();
    const qint64 pending = m_session->pendingBytes();
    m_bandwidthEstimator.onBackpressureSample(pending, nowMs);

    if (!m_session->canSendScreenFrame())
        return;

    const qint64 elapsedSinceLastSend = m_lastSentTimer.isValid()
            ? m_lastSentTimer.elapsed()
            : m_nextCaptureDelayMs;
    if (elapsedSinceLastSend < m_nextCaptureDelayMs)
        return;

    const QImage frame = m_frameSource ? m_frameSource() : m_capture.capture();
    if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0) {
        qWarning() << "[ScreenStreamSender] Chụp khung hình thất bại.";
        return;
    }

    const std::optional<double> cpuOpt = sampleCpuUsage(nowMs);
    const quint64 epoch = m_sessionEpoch;
    const quint64 sessionId = m_currentSessionId;
    const quint32 frameId = m_frameId;
    const bool ackEnabled = m_session->isScreenFrameAcksEnabled();
    const double ackLatencyMs = m_lastAckLatencyMs;
    const double estBw = m_bandwidthEstimator.estimatedBandwidthBps();
    const double availBw = m_bandwidthEstimator.availableBandwidthBps();
    const double obsTp = m_bandwidthEstimator.observedThroughputBps();
    const double smoothRtt = m_bandwidthEstimator.smoothedRttMs();
    const double minRtt = m_bandwidthEstimator.minRttMs();
    const bool congested = m_bandwidthEstimator.isCongested();
    const double actualFpsNow = m_metrics.actualFps(nowMs);

    m_workerBusy = true;
    m_encodingFrame = frame;
    m_encodingSessionId = sessionId;

    m_encoder.setFuture(QtConcurrent::run([this,
                                           epoch,
                                           sessionId,
                                           frameId,
                                           nowMs,
                                           frame,
                                           pending,
                                           ackEnabled,
                                           ackLatencyMs,
                                           estBw,
                                           availBw,
                                           obsTp,
                                           smoothRtt,
                                           minRtt,
                                           congested,
                                           actualFpsNow,
                                           cpuOpt]() -> WorkerResult {
        WorkerResult res;
        res.sessionEpoch = epoch;
        res.sessionId = sessionId;
        res.frameId = frameId;
        res.captureNowMs = nowMs;

        res.motion = m_motionDetector.analyze(frame);

        AdaptiveStreamController::FrameInput input;
        input.frameId = frameId;
        input.frame = frame;
        input.nowMs = nowMs;
        input.motionScore = res.motion.smoothedScore;
        input.wQ = res.motion.wQ;
        input.wR = res.motion.wR;
        input.wP = res.motion.wP;
        input.estimatedBandwidthBps = estBw;
        input.availableBandwidthBps = availBw;
        input.observedThroughputBps = obsTp;
        input.smoothedRttMs = smoothRtt;
        input.minRttMs = minRtt;
        input.networkCongested = congested;
        input.pendingBytes = pending;
        input.ackLatencyMs = ackLatencyMs;
        input.ackEnabled = ackEnabled;
        input.actualFps = actualFpsNow;
        input.cpuUsagePercent = cpuOpt;

        res.decision = m_controller.processFrame(input);
        return res;
    }));
}

void ScreenStreamSender::sendEncodedFrame()
{
    m_workerBusy = false;
    QImage frame = std::exchange(m_encodingFrame, {});
    const WorkerResult out = m_encoder.result();

    if (out.sessionEpoch != m_sessionEpoch
        || m_currentSessionId == 0
        || out.sessionId != m_currentSessionId
        || m_encodingSessionId != m_currentSessionId
        || !m_session) {
        return;
    }

    const qint64 nowMs = currentNowMs();
    StreamMetricsSnapshot snap = out.decision.metricsSnapshot;
    const double availBw = m_bandwidthEstimator.availableBandwidthBps();

    if (!out.decision.shouldSend || out.decision.dropped || out.decision.jpeg.isEmpty()) {
        m_metrics.recordDroppedFrame();
        snap.actualFrameBytes = 0;
        snap.pendingBytes = m_session->pendingBytes();
        snap.droppedFrames = m_metrics.droppedFrames();
        snap.actualFps = m_metrics.actualFps(nowMs);
        {
            QMutexLocker locker(&m_snapshotMutex);
            m_latestSnapshot = snap;
        }
        m_metrics.maybeLog(snap, nowMs);

        const int fallbackFps = qMax(1, out.decision.targetFps > 0 ? out.decision.targetFps : 2);
        m_nextCaptureDelayMs = qMax<qint64>(40, 1000 / fallbackFps - out.decision.encodeTimeMs);
        m_lastSentTimer.start();
        ++m_frameId;
        return;
    }

    const QList<QByteArray> packets =
        ScreenFramePacketizer::packetize(out.decision.jpeg, m_frameId, m_currentSessionId);
    if (packets.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Packetize returned empty, skipping frame.";
        return;
    }

    qint64 frameBytes = 0;
    for (const QByteArray &packet : packets) {
        frameBytes += packet.size();
    }

    const qsizetype maxWireAllowed = m_controller.effectiveMaxWireBytes();
    if (frameBytes <= 0 || frameBytes > maxWireAllowed) {
        m_metrics.recordDroppedFrame();
        snap.actualFrameBytes = 0;
        snap.pendingBytes = m_session->pendingBytes();
        snap.droppedFrames = m_metrics.droppedFrames();
        snap.actualFps = m_metrics.actualFps(nowMs);
        snap.configChangeReason = QStringLiteral("frame_drop");
        {
            QMutexLocker locker(&m_snapshotMutex);
            m_latestSnapshot = snap;
        }
        m_metrics.maybeLog(snap, nowMs);
        ++m_frameId;
        return;
    }

    if (!m_session->sendScreenFrame(m_frameId, packets)) {
        return;
    }

    const qint64 sentNowMs = currentNowMs();
    const qint64 pendingAfter = m_session->pendingBytes();
    m_bandwidthEstimator.onFrameSent(m_frameId,
                                     static_cast<qsizetype>(frameBytes),
                                     out.decision.encodeTimeMs,
                                     pendingAfter,
                                     sentNowMs);
    m_metrics.recordFrameSent(sentNowMs);

    snap.actualFrameBytes = static_cast<qsizetype>(frameBytes);
    snap.pendingBytes = pendingAfter;
    snap.droppedFrames = m_metrics.droppedFrames();
    snap.actualFps = m_metrics.actualFps(sentNowMs);
    {
        QMutexLocker locker(&m_snapshotMutex);
        m_latestSnapshot = snap;
    }
    m_metrics.maybeLog(snap, sentNowMs);

    m_lastSentFrame = std::move(frame);
    // m_lastSentTimer starts at send time and runs concurrently with ACK flight time,
    // so passing ackAlreadyWaited=false gives total interval from send to next capture
    // without double-counting RTT.
    m_nextCaptureDelayMs = AdaptiveStreamController::computeNextFrameDelayMs(
        static_cast<qsizetype>(frameBytes),
        out.decision.encodeTimeMs,
        0.0,
        availBw,
        out.decision.targetFps,
        false);
    m_lastSentTimer.start();
    ++m_frameId;
    emit frameCaptured(out.decision.encodedSize);
}
