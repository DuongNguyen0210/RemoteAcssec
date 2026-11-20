#include "ScreenStreamSender.h"
#include "Network/Relay/ChildSessionController.h"

#include "ScreenCapture.h"
#include "ScreenEncoder.h"
#include "Network/Protocol/ScreenFramePacketizer.h"

#include <QDebug>

ScreenStreamSender::ScreenStreamSender(ChildSessionController *session, QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_session(session)
    , m_frameId(1)
    , m_currentSessionId(0)
{
    m_timer->setInterval(STREAM_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &ScreenStreamSender::onTick);
    connect(session, &ChildSessionController::sessionStarted, this, [this](quint64 id) {
        m_currentSessionId = id;
        m_lastSentFrame = QImage();
        m_lastEncodedFrame = {};
        m_lastSentTimer.invalidate();
        m_timer->start();
    });
    connect(session, &ChildSessionController::sessionEnded, this, [this] {
        m_currentSessionId = 0;
        m_lastSentFrame = QImage();
        m_lastEncodedFrame = {};
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
    if (m_currentSessionId == 0)
        return;

    const qint64 pending = m_session->pendingBytes();
    if (pending > 0) {
        return;
    }

    const QImage frame = m_capture.capture();
    if (frame.isNull()) {
        qWarning() << "[ScreenStreamSender] Chụp khung hình thất bại.";
        return;
    }

    const FrameChange change = classifyFrameChange(frame, m_lastSentFrame);
    const qint64 elapsedSinceLastSend = m_lastSentTimer.isValid()
            ? m_lastSentTimer.elapsed() : KEEPALIVE_FRAME_INTERVAL_MS;
    const int keepAliveInterval = qEnvironmentVariable("PBL4_ISOLATED_X11") == QStringLiteral("1")
            ? 400 : KEEPALIVE_FRAME_INTERVAL_MS;

    if (change == FrameChange::None && elapsedSinceLastSend < keepAliveInterval) {
        return;
    }
    if (change == FrameChange::Minor && elapsedSinceLastSend < MINOR_CHANGE_INTERVAL_MS) {
        return;
    }

    EncodedScreenFrame encoded;
    if (change == FrameChange::None && !m_lastEncodedFrame.jpeg.isEmpty()) {
        encoded = m_lastEncodedFrame;
    } else {
        encoded = ScreenEncoder::encodeWithinLimit(frame, MAX_ENCODED_FRAME_BYTES);
    }
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
    if (pending + frameBytes > MAX_PENDING_BYTES) return;
    emit frameCaptured(encoded.size);

    for (const QByteArray &pkt : packets) {
        const qint64 written = m_session->sendScreenPacket(pkt);
        if (written < 0) {
            qWarning() << "[ScreenStreamSender] Session rejected the screen packet."
                       << "Aborting frame" << m_frameId;
            return;
        }
    }

    if (change != FrameChange::None || m_lastSentFrame.isNull()) {
        m_lastSentFrame = frame;
        m_lastEncodedFrame = encoded;
    }
    m_lastSentTimer.start();
    ++m_frameId;
}
