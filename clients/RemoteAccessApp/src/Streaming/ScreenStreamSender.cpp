#include "ScreenStreamSender.h"
#include "Network/Relay/ChildSessionController.h"

#include "Network/Protocol/ScreenFramePacketizer.h"

#include <QDebug>
#include <algorithm>
#include <limits>
#include <QtConcurrent/QtConcurrentRun>
#include <utility>

namespace {
int streamBytesPerSecond()
{
    bool valid = false;
    const int configuredKbps = qEnvironmentVariableIntValue("REMOTE_STREAM_KBPS", &valid);
    return (valid ? qBound(128, configuredKbps, 12000) : 1000) * 125;
}
}

ScreenStreamSender::ScreenStreamSender(ChildSessionController *session, QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_session(session)
    , m_frameId(1)
    , m_currentSessionId(0)
    , m_rate(streamBytesPerSecond())
{
    m_clock.start();
    m_timer->setSingleShot(true);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &ScreenStreamSender::onTick);
    connect(&m_encoder, &QFutureWatcherBase::finished, this, &ScreenStreamSender::encodingFinished);
    connect(session, &ChildSessionController::sessionStarted, this, &ScreenStreamSender::resetSession);
    connect(session, &ChildSessionController::sessionEnded, this, [this] { resetSession(0); });
    connect(session, &ChildSessionController::screenFrameAcknowledged, this,
            [this](quint64 id) {
                if (id == m_currentSessionId && !m_workerActive) schedule(0);
            });
}

ScreenStreamSender::~ScreenStreamSender() = default;

void ScreenStreamSender::resetSession(quint64 sessionId)
{
    m_timer->stop();
    m_currentSessionId = sessionId;
    m_encodingSessionId = 0;
    m_lastSentFrame = {};
    m_encodingFrame = {};
    m_prepared.reset();
    m_search = {};
    m_rate.reset();
    m_workMs = 0;
    if (sessionId) schedule(0);
}

void ScreenStreamSender::schedule(qint64 delayMs)
{
    if (m_currentSessionId)
        m_timer->start(int(std::clamp<qint64>(delayMs, 0, std::numeric_limits<int>::max())));
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
    if (!m_currentSessionId || m_workerActive) return;
    const qint64 now = m_clock.elapsed();
    if (!m_session->canSendScreenFrame()) {
        schedule(STREAM_INTERVAL_MS);
        return;
    }
    if (m_prepared) {
        sendPreparedFrame();
        return;
    }

    // Start capture just before the send deadline. Only one JPEG can wait for it.
    const qint64 untilCapture = m_rate.nextSendMs() - now - m_workMs;
    if (untilCapture > 0) {
        schedule(untilCapture);
        return;
    }
    const QImage frame = m_capture.capture();
    if (frame.isNull()) {
        schedule(STREAM_INTERVAL_MS);
        return;
    }

    const int refreshInterval = qEnvironmentVariable("PBL4_ISOLATED_X11") == QStringLiteral("1")
            ? 400 : REFRESH_INTERVAL_MS;
    const qint64 lastSend = m_rate.lastSendMs();
    const qint64 sinceLastSend = lastSend < 0 ? refreshInterval : now - lastSend;
    constexpr qsizetype overhead = Protocol::HEADER_SIZE + ScreenFramePacketizer::SCREEN_FRAME_METADATA_SIZE;
    static_assert(MAX_WIRE_FRAME_BYTES - overhead <= ScreenFramePacketizer::maxChunkData());
    const qsizetype budget = m_rate.wireBudget() - overhead;
    const auto state = m_search;
    const bool probe = m_rate.canProbe(now);
    m_captureStartMs = now;
    m_encodingFrame = frame;
    m_encodingSessionId = m_currentSessionId;
    m_workerActive = true;
    m_encoder.setFuture(QtConcurrent::run([frame, previous = m_lastSentFrame, budget, state, probe,
                                           sinceLastSend, refreshInterval]() -> std::optional<EncodedScreenFrame> {
        // Scanning a high-resolution desktop competes with input handling on the GUI thread.
        const auto change = classifyFrameChange(frame, previous);
        if ((change == FrameChange::None && sinceLastSend < refreshInterval)
            || (change == FrameChange::Minor && sinceLastSend < MINOR_CHANGE_INTERVAL_MS))
            return std::nullopt;
        return ScreenEncoder::encodeWithinLimit(frame, budget, state, probe);
    }));
}

void ScreenStreamSender::encodingFinished()
{
    m_workerActive = false;
    if (m_encodingSessionId == 0 || m_encodingSessionId != m_currentSessionId) {
        schedule(0);
        return;
    }
    m_prepared = m_encoder.result();
    if (!m_prepared) {
        m_encodingFrame = {};
        schedule(STREAM_INTERVAL_MS);
        return;
    }
    m_workMs = m_clock.elapsed() - m_captureStartMs;
    onTick();
}

void ScreenStreamSender::sendPreparedFrame()
{
    const qint64 now = m_clock.elapsed();
    const qint64 untilSend = m_rate.nextSendMs() - now;
    if (untilSend > 0) {
        schedule(untilSend);
        return;
    }

    auto encoded = std::move(*m_prepared);
    m_prepared.reset();
    QImage source = std::exchange(m_encodingFrame, {});
    const auto packets = encoded.jpeg.isEmpty() ? QList<QByteArray>{}
        : ScreenFramePacketizer::packetize(encoded.jpeg, m_frameId, m_currentSessionId);
    qint64 wireBytes = 0;
    for (const auto &packet : packets) wireBytes += packet.size();
    if (packets.isEmpty() || wireBytes > m_rate.wireBudget()) {
        m_search = encoded.nextState;
        schedule(STREAM_INTERVAL_MS);
        return;
    }
    if (!m_session->sendScreenFrame(m_frameId, packets)) {
        schedule(STREAM_INTERVAL_MS);
        return;
    }
    m_search = encoded.nextState;
    m_rate.sent(now, wireBytes);
    m_lastSentFrame = std::move(source);
    ++m_frameId;
    schedule(m_rate.nextSendMs() - now - m_workMs);
    emit frameCaptured(encoded.size);
}
