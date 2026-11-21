#include "ScreenStreamSender.h"
#include "Network/Relay/ChildSessionController.h"

#include "Network/Protocol/ScreenFramePacketizer.h"

#include <QDebug>
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
    , m_wireBytesPerSecond(streamBytesPerSecond())
{
    m_timer->setInterval(STREAM_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &ScreenStreamSender::onTick);
    connect(&m_encoder, &QFutureWatcher<EncodedScreenFrame>::finished, this, &ScreenStreamSender::sendEncodedFrame);
    connect(session, &ChildSessionController::sessionStarted, this, [this](quint64 id) {
        m_currentSessionId = id;
        m_lastSentFrame = QImage();
        m_encodingSessionId = 0;
        m_lastSentTimer.invalidate();
        m_nextCaptureDelayMs = 0;
        m_timer->start();
    });
    connect(session, &ChildSessionController::sessionEnded, this, [this] {
        m_currentSessionId = 0;
        m_lastSentFrame = QImage();
        m_encodingSessionId = 0;
        m_lastSentTimer.invalidate();
        m_timer->stop();
    });
}

ScreenStreamSender::~ScreenStreamSender() = default;

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
    if (m_currentSessionId == 0 || !m_encodingFrame.isNull() || !m_session->canSendScreenFrame())
        return;

    const qint64 elapsedSinceLastSend = m_lastSentTimer.isValid()
            ? m_lastSentTimer.elapsed() : REFRESH_INTERVAL_MS;
    if (elapsedSinceLastSend < m_nextCaptureDelayMs)
        return;

    const QImage frame = m_capture.capture();
    if (frame.isNull()) {
        qWarning() << "[ScreenStreamSender] Chụp khung hình thất bại.";
        return;
    }

    const FrameChange change = classifyFrameChange(frame, m_lastSentFrame);
    const int refreshInterval = qEnvironmentVariable("PBL4_ISOLATED_X11") == QStringLiteral("1")
            ? 400 : REFRESH_INTERVAL_MS;
    if (change == FrameChange::None && elapsedSinceLastSend < refreshInterval) {
        return;
    }
    if (change == FrameChange::Minor && elapsedSinceLastSend < MINOR_CHANGE_INTERVAL_MS) {
        return;
    }

    // Bound one burst to 200 ms of the configured rate during motion and
    // 500 ms for a sharper refresh. A fresh capture includes quiet changes
    // the pixel sampler may have missed.
    const qsizetype burst = m_wireBytesPerSecond / (change == FrameChange::Significant ? 5 : 2);
    constexpr qsizetype overhead = Protocol::HEADER_SIZE + ScreenFramePacketizer::SCREEN_FRAME_METADATA_SIZE;
    static_assert(MAX_WIRE_FRAME_BYTES - overhead <= ScreenFramePacketizer::maxChunkData());
    const qsizetype jpegBudget = qMin(burst, MAX_WIRE_FRAME_BYTES) - overhead;
    m_encodingFrame = frame;
    m_encodingSessionId = m_currentSessionId;
    m_encoder.setFuture(QtConcurrent::run([frame, jpegBudget] {
        return ScreenEncoder::encodeWithinLimit(frame, jpegBudget);
    }));
}

void ScreenStreamSender::sendEncodedFrame()
{
    const auto encoded = m_encoder.result();
    QImage frame = std::exchange(m_encodingFrame, {});
    if (m_encodingSessionId == 0 || m_encodingSessionId != m_currentSessionId)
        return;
    if (encoded.jpeg.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Mã hoá JPEG thất bại.";
        return;
    }

    const QList<QByteArray> packets =
        ScreenFramePacketizer::packetize(encoded.jpeg, m_frameId, m_currentSessionId);
    if (packets.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Packetize returned empty, skipping frame.";
        return;
    }

    qint64 frameBytes = 0;
    for (const QByteArray &packet : packets) frameBytes += packet.size();
    if (!m_session->sendScreenFrame(m_frameId, packets))
        return;

    m_lastSentFrame = std::move(frame);
    m_nextCaptureDelayMs = (frameBytes * 1000 + m_wireBytesPerSecond - 1) / m_wireBytesPerSecond;
    m_lastSentTimer.start();
    ++m_frameId;
    emit frameCaptured(encoded.size);
}
