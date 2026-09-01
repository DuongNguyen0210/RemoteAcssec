#include "ScreenStreamReceiver.h"

#include <QDebug>

ScreenStreamReceiver::ScreenStreamReceiver(QObject *parent)
    : QObject(parent)
    , m_activeSessionId(0)
{
}

void ScreenStreamReceiver::setActiveSession(quint64 sessionId)
{
    m_reassembler.reset();
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

    const auto completeFrame = m_reassembler.addChunk(
            message.payload, message.header.sequenceNumber);
    if (!completeFrame.has_value())
        return;

    const QImage image = QImage::fromData(completeFrame->jpegBytes, "JPEG");
    if (image.isNull()) {
        qWarning() << "[ScreenStreamReceiver] Giai ma JPEG that bai, frameId="
                   << completeFrame->frameId;
        return;
    }

    emit frameReady(image);
}
