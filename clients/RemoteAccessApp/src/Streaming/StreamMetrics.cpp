#include "StreamMetrics.h"

#include <QDebug>
#include <QMutexLocker>

#include <algorithm>
#include <utility>

StreamMetrics::StreamMetrics(qint64 logIntervalMs)
    : m_logIntervalMs(std::max<qint64>(0, logIntervalMs))
{
    m_clock.start();
}

void StreamMetrics::setLogIntervalMs(qint64 intervalMs)
{
    QMutexLocker locker(&m_mutex);
    m_logIntervalMs = std::max<qint64>(0, intervalMs);
}

qint64 StreamMetrics::logIntervalMs() const
{
    QMutexLocker locker(&m_mutex);
    return m_logIntervalMs;
}

void StreamMetrics::setLogSink(LogSink sink)
{
    QMutexLocker locker(&m_mutex);
    m_sink = std::move(sink);
}

void StreamMetrics::reset()
{
    QMutexLocker locker(&m_mutex);
    m_clock.restart();
    m_hasLogged = false;
    m_lastLogMs = 0;
    m_logCount = 0;
    m_droppedFrames = 0;
    m_pendingChangeReason.clear();
    m_lastFormattedLine.clear();
    m_sentTimestampsMs.clear();
}

void StreamMetrics::pruneOldFpsSamplesLocked(qint64 nowMs) const
{
    while (!m_sentTimestampsMs.empty()
           && (nowMs - m_sentTimestampsMs.front() > DEFAULT_FPS_WINDOW_MS))
    {
        m_sentTimestampsMs.pop_front();
    }
}

void StreamMetrics::recordFrameSent(qint64 nowMs)
{
    QMutexLocker locker(&m_mutex);
    if (!m_sentTimestampsMs.empty() && nowMs < m_sentTimestampsMs.back())
    {
        m_sentTimestampsMs.clear();
    }
    m_sentTimestampsMs.push_back(nowMs);
    pruneOldFpsSamplesLocked(nowMs);
}

void StreamMetrics::recordDroppedFrame()
{
    QMutexLocker locker(&m_mutex);
    ++m_droppedFrames;
}

qint64 StreamMetrics::droppedFrames() const
{
    QMutexLocker locker(&m_mutex);
    return m_droppedFrames;
}

double StreamMetrics::actualFps(qint64 nowMs) const
{
    QMutexLocker locker(&m_mutex);
    pruneOldFpsSamplesLocked(nowMs);
    if (m_sentTimestampsMs.size() < 2)
    {
        return 0.0;
    }

    const qint64 spanMs = std::max<qint64>(1, nowMs - m_sentTimestampsMs.front());
    const double intervals = static_cast<double>(m_sentTimestampsMs.size() - 1);
    return (intervals * 1000.0) / static_cast<double>(spanMs);
}

bool StreamMetrics::maybeLog(const StreamMetricsSnapshot &snapshot, qint64 nowMs)
{
    QString line;
    LogSink sink;

    {
        QMutexLocker locker(&m_mutex);

        if (!snapshot.configChangeReason.isEmpty())
        {
            m_pendingChangeReason = snapshot.configChangeReason;
        }

        const bool shouldEmit = !m_hasLogged
            || m_logIntervalMs <= 0
            || (nowMs - m_lastLogMs >= m_logIntervalMs);
        if (!shouldEmit)
        {
            return false;
        }

        if (snapshot.configChangeReason.isEmpty() && !m_pendingChangeReason.isEmpty())
        {
            StreamMetricsSnapshot effective = snapshot;
            effective.configChangeReason = m_pendingChangeReason;
            line = formatSnapshot(effective);
        }
        else
        {
            line = formatSnapshot(snapshot);
        }

        m_pendingChangeReason.clear();
        m_hasLogged = true;
        m_lastLogMs = nowMs;
        ++m_logCount;
        m_lastFormattedLine = line;
        sink = m_sink;
    }

    if (sink)
    {
        sink(line);
    }
    else
    {
        qDebug().noquote() << line;
    }
    return true;
}

bool StreamMetrics::maybeLog(const StreamMetricsSnapshot &snapshot)
{
    qint64 elapsedMs = 0;
    {
        QMutexLocker locker(&m_mutex);
        elapsedMs = m_clock.elapsed();
    }
    return maybeLog(snapshot, elapsedMs);
}

qint64 StreamMetrics::logCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_logCount;
}

QString StreamMetrics::lastFormattedLine() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastFormattedLine;
}

QString StreamMetrics::formatSnapshot(const StreamMetricsSnapshot &s)
{
    const QString reason = s.configChangeReason.isEmpty()
        ? QStringLiteral("steady")
        : s.configChangeReason;
    const QString resStr = s.resolution.isValid()
        ? QStringLiteral("%1x%2").arg(s.resolution.width()).arg(s.resolution.height())
        : QStringLiteral("0x0");

    return QStringLiteral(
        "[StreamMetrics] estBwBps=%1 obsThroughputBps=%2 byteBudget=%3 frameBytes=%4 "
        "motion=%5 quality=%6 res=%7 targetFps=%8 actualFps=%9 encodeMs=%10 "
        "retries=%11 ackRttMs=%12 pendingBytes=%13 dropped=%14 reason=\"%15\"")
        .arg(s.estimatedBandwidthBps, 0, 'f', 1)
        .arg(s.observedThroughputBps, 0, 'f', 1)
        .arg(s.frameByteBudget)
        .arg(s.actualFrameBytes)
        .arg(s.motionScore, 0, 'f', 3)
        .arg(s.jpegQuality)
        .arg(resStr)
        .arg(s.targetFps)
        .arg(s.actualFps, 0, 'f', 2)
        .arg(s.encodeTimeMs)
        .arg(s.encodeRetryCount)
        .arg(s.ackLatencyMs, 0, 'f', 1)
        .arg(s.pendingBytes)
        .arg(s.droppedFrames)
        .arg(reason);
}
