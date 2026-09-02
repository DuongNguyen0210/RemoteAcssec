#include "ScreenStreamReceiver.h"

#include <QDebug>
#include <QMetaObject>

#include <utility>

ScreenStreamReceiver::ScreenStreamReceiver(QObject *parent)
    : QObject(parent)
    , m_activeSessionId(0)
{
}

void ScreenStreamReceiver::setActiveSession(quint64 sessionId)
{
    m_reassembler.reset();
    m_pendingJpeg.clear();
    m_pendingFrameId = 0;
    m_activeSessionId = sessionId;
}

void ScreenStreamReceiver::handleMessage(
        const Protocol::RdtpStreamParser::Message &message)
{
    if (message.header.type != Protocol::MessageType::SCREEN_FRAME)
        return;

    if (message.header.flags != 0) {
        qWarning() << "[ScreenStreamReceiver] Bo qua SCREEN_FRAME co flags khong hop le.";
        return;
    }

    if (m_activeSessionId == 0
            || message.header.sessionId == 0
            || message.header.sessionId != m_activeSessionId) {
        qWarning() << "[ScreenStreamReceiver] Bo qua SCREEN_FRAME khong dung phien.";
        return;
    }

    if (message.header.payloadLength != static_cast<uint32_t>(message.payload.size())) {
        qWarning() << "[ScreenStreamReceiver] Bo qua SCREEN_FRAME sai payloadLength.";
        return;
    }

    auto completeFrame = m_reassembler.addChunk(
            message.payload, message.header.sequenceNumber);
    if (!completeFrame.has_value())
        return;

    m_pendingJpeg = std::move(completeFrame->jpegBytes);
    m_pendingFrameId = completeFrame->frameId;

    if (m_decodeScheduled)
        return;

    m_decodeScheduled = true;
    QMetaObject::invokeMethod(
            this,
            [this]() { decodePendingFrame(); },
            Qt::QueuedConnection);
}

void ScreenStreamReceiver::decodePendingFrame()
{
    QByteArray jpegBytes = std::move(m_pendingJpeg);
    const uint32_t frameId = m_pendingFrameId;
    m_pendingJpeg.clear();
    m_pendingFrameId = 0;
    m_decodeScheduled = false;

    if (jpegBytes.isEmpty())
        return;

    const QImage image = QImage::fromData(jpegBytes, "JPEG");
    if (image.isNull()) {
        qWarning() << "[ScreenStreamReceiver] Giai ma JPEG that bai, frameId="
                   << frameId;
        return;
    }

    emit frameReady(image);
}
