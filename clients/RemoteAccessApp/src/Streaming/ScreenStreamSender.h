#ifndef SCREENSTREAMSENDER_H
#define SCREENSTREAMSENDER_H

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <cstdint>
#include <functional>
#include <optional>

#include "AdaptiveStreamController.h"
#include "BandwidthEstimator.h"
#include "MotionDetector.h"
#include "ScreenCapture.h"
#include "ScreenEncoder.h"
#include "StreamMetrics.h"
#include "Utils/SystemTelemetrySampler.h"

class ChildSessionController;

class ScreenStreamSender : public QObject
{
    Q_OBJECT

public:
    enum class FrameChange {
        None,
        Minor,
        Significant
    };

    static constexpr int STREAM_INTERVAL_MS = 100;
    static constexpr int DEFAULT_TICK_INTERVAL_MS = 20;
    static constexpr int MINOR_CHANGE_INTERVAL_MS = 250;
    static constexpr int REFRESH_INTERVAL_MS = 1200;
    static constexpr qsizetype MAX_WIRE_FRAME_BYTES = 256 * 1024;

    static FrameChange classifyFrameChange(const QImage &current, const QImage &previous);

    explicit ScreenStreamSender(ChildSessionController *session, QObject *parent = nullptr);
    ~ScreenStreamSender() override;

    // Testability & runtime configuration seams (M3 / E2E / M4 benchmark)
    void setFrameSource(std::function<QImage()> source);
    void setCpuUsageSource(std::function<double()> source);

    void setControllerConfig(const AdaptiveStreamController::Config &config);
    AdaptiveStreamController::Config controllerConfig() const;

    void setBandwidthEstimatorConfig(const BandwidthEstimator::Config &config);
    BandwidthEstimator::Config bandwidthEstimatorConfig() const;

    void setMotionDetectorConfig(const MotionDetector::Config &config);
    MotionDetector::Config motionDetectorConfig() const;

    void setMetricsLogIntervalMs(qint64 intervalMs);
    qint64 metricsLogIntervalMs() const;

    StreamMetricsSnapshot latestMetricsSnapshot() const;
    StreamConfig activeStreamConfig() const;
    double estimatedBandwidthBps() const;
    double availableBandwidthBps() const;
    double observedThroughputBps() const;
    double smoothedRttMs() const;
    double smoothedMotionScore() const;
    qint64 droppedFramesCount() const;

signals:
    void frameCaptured(const QSize &size);

private slots:
    void onTick();
    void sendEncodedFrame();

private:
    struct WorkerResult
    {
        quint64 sessionEpoch = 0;
        quint64 sessionId = 0;
        quint32 frameId = 0;
        qint64 captureNowMs = 0;
        MotionResult motion;
        AdaptiveStreamController::FrameDecision decision;
    };

    qint64 currentNowMs() const;
    std::optional<double> sampleCpuUsage(qint64 nowMs);

    QTimer *m_timer = nullptr;
    ChildSessionController *m_session = nullptr;
    uint32_t m_frameId = 1;
    uint64_t m_currentSessionId = 0;
    uint64_t m_sessionEpoch = 0;

    ScreenCapture m_capture;
    std::function<QImage()> m_frameSource;
    std::function<double()> m_cpuUsageSource;
    SystemTelemetrySampler m_telemetrySampler;
    qint64 m_lastCpuSampleMs = -1000;
    std::optional<double> m_cachedCpuPercent;

    MotionDetector m_motionDetector;
    BandwidthEstimator m_bandwidthEstimator;
    AdaptiveStreamController m_controller;
    StreamMetrics m_metrics;

    QImage m_lastSentFrame;
    QImage m_encodingFrame;
    quint64 m_encodingSessionId = 0;
    bool m_workerBusy = false;
    QFutureWatcher<WorkerResult> m_encoder;

    QElapsedTimer m_clock;
    QElapsedTimer m_lastSentTimer;
    qint64 m_nextCaptureDelayMs = 0;
    double m_lastAckLatencyMs = -1.0;

    mutable QMutex m_snapshotMutex;
    StreamMetricsSnapshot m_latestSnapshot;
};

#endif
