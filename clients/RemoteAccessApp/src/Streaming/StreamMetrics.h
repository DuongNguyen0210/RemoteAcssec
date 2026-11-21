#pragma once

#include <QElapsedTimer>
#include <QMutex>
#include <QSize>
#include <QString>

#include <deque>
#include <functional>

struct StreamMetricsSnapshot
{
    double estimatedBandwidthBps = 0.0;
    double observedThroughputBps = 0.0;
    qsizetype frameByteBudget = 0;
    qsizetype actualFrameBytes = 0;
    double motionScore = 0.0;
    int jpegQuality = 0;
    QSize resolution;
    int targetFps = 0;
    double actualFps = 0.0;
    qint64 encodeTimeMs = 0;
    int encodeRetryCount = 0;
    double ackLatencyMs = -1.0;
    qint64 pendingBytes = 0;
    qint64 droppedFrames = 0;
    QString configChangeReason;
};

class StreamMetrics
{
public:
    using LogSink = std::function<void(const QString &)>;

    static constexpr qint64 DEFAULT_LOG_INTERVAL_MS = 1000;
    static constexpr qint64 DEFAULT_FPS_WINDOW_MS = 2000;

    explicit StreamMetrics(qint64 logIntervalMs = DEFAULT_LOG_INTERVAL_MS);

    void setLogIntervalMs(qint64 intervalMs);
    qint64 logIntervalMs() const;

    void setLogSink(LogSink sink);
    void reset();

    void recordFrameSent(qint64 nowMs);
    void recordDroppedFrame();
    qint64 droppedFrames() const;
    double actualFps(qint64 nowMs) const;

    bool maybeLog(const StreamMetricsSnapshot &snapshot, qint64 nowMs);
    bool maybeLog(const StreamMetricsSnapshot &snapshot);

    qint64 logCount() const;
    QString lastFormattedLine() const;

    static QString formatSnapshot(const StreamMetricsSnapshot &snapshot);

private:
    void pruneOldFpsSamplesLocked(qint64 nowMs) const;

    mutable QMutex m_mutex;
    qint64 m_logIntervalMs = DEFAULT_LOG_INTERVAL_MS;
    QElapsedTimer m_clock;
    bool m_hasLogged = false;
    qint64 m_lastLogMs = 0;
    qint64 m_logCount = 0;
    qint64 m_droppedFrames = 0;
    QString m_pendingChangeReason;
    QString m_lastFormattedLine;
    mutable std::deque<qint64> m_sentTimestampsMs;
    LogSink m_sink;
};
