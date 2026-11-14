#include "ScreenStreamSender.h"
#include "Network/Relay/ChildSessionController.h"

#include "ScreenCapture.h"
#include "ScreenEncoder.h"
#include "Network/Relay/RelayClient.h"
#include "Network/Protocol/ScreenFramePacketizer.h"

#include <QDebug>

ScreenStreamSender::ScreenStreamSender(ChildSessionController *session, QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_relayClient(session->relayClient())
    , m_frameId(1)
    , m_currentSessionId(0)
{
    m_timer->setInterval(STREAM_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &ScreenStreamSender::onTick);
    connect(session, &ChildSessionController::sessionStarted, this, [this](quint64 id) {
        m_currentSessionId = id;
        m_timer->start();
    });
    connect(session, &ChildSessionController::sessionEnded, this, [this] {
        m_currentSessionId = 0;
        m_timer->stop();
    });
}

ScreenStreamSender::~ScreenStreamSender() = default;

void ScreenStreamSender::onTick()
{
    if (m_currentSessionId == 0)
        return;

    const qint64 pending = m_relayClient->pendingBytes();
    if (pending > MAX_PENDING_BYTES) {
        return;
    }

    const QImage frame = m_capture.capture();
    if (frame.isNull()) {
        qWarning() << "[ScreenStreamSender] Chụp khung hình thất bại.";
        return;
    }
    emit frameCaptured(frame.size());

    const QByteArray encoded = ScreenEncoder::encodeJpeg(frame);
    if (encoded.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Mã hoá JPEG thất bại.";
        return;
    }

    const QList<QByteArray> packets =
        ScreenFramePacketizer::packetize(encoded, m_frameId, m_currentSessionId);
    if (packets.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Packetize returned empty, skipping frame.";
        return;
    }

    for (const QByteArray &pkt : packets) {
        const qint64 written = m_relayClient->sendRawPacket(pkt);
        if (written < 0) {
            qWarning() << "[ScreenStreamSender] sendRawPacket failed (not connected?)."
                       << "Aborting frame" << m_frameId;
            return;
        }
    }

    ++m_frameId;
}
