#include "ScreenStreamSender.h"

#include "ScreenCapture.h"
#include "ScreenEncoder.h"
#include "../../Network/relayclient.h"
#include "../../Network/protocol/ScreenFramePacketizer.h"

#include <QDebug>

// Relay host for local / LAN testing.
static const QString RELAY_HOST = QStringLiteral("localhost");

ScreenStreamSender::ScreenStreamSender(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_relayClient(new RelayClient(this))
    , m_frameId(1)
{
    m_timer->setInterval(STREAM_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &ScreenStreamSender::onTick);
}

ScreenStreamSender::~ScreenStreamSender()
{
    stop();
}

// ---------------------------------------------------------------------------
// start()
// Connect to the Relay Proxy once then start the repeating capture timer.
// ---------------------------------------------------------------------------

void ScreenStreamSender::start()
{
    m_relayClient->ConnectToServer(RELAY_HOST, RELAY_PORT);
    m_timer->start();
    qDebug() << "[ScreenStreamSender] Started."
             << "host=" << RELAY_HOST
             << "port=" << RELAY_PORT
             << "interval=" << STREAM_INTERVAL_MS << "ms";
}

void ScreenStreamSender::stop()
{
    if (m_timer->isActive()) {
        m_timer->stop();
        qDebug() << "[ScreenStreamSender] Stopped.";
    }
}

// ---------------------------------------------------------------------------
// onTick()
// Called by the timer every STREAM_INTERVAL_MS.
//
// Policy:
//  1. Backpressure check — skip frame if socket output queue is too large.
//  2. Capture one frame.
//  3. JPEG encode.
//  4. Packetize into RDTP SCREEN_FRAME messages.
//  5. Send each packet via RelayClient::sendRawPacket().
//  6. Increment frameId once per successfully packetized frame.
//
// Any failure at steps 1-4 silently skips the current tick.
// A failure at step 5 (sendRawPacket returns -1) aborts sending the rest
// of the current frame's chunks and skips the frameId increment.
// ---------------------------------------------------------------------------

void ScreenStreamSender::onTick()
{
    // --- 1. Backpressure check ---------------------------------------------
    const qint64 pending = m_relayClient->pendingBytes();
    if (pending > MAX_PENDING_BYTES) {
        qDebug() << "[ScreenStreamSender] Backpressure: skipping frame"
                 << m_frameId << " pending=" << pending << "bytes";
        return;
    }

    // --- 2. Capture --------------------------------------------------------
    static ScreenCapture capture;   // stateless — reuse across ticks
    QImage frame = capture.capture();
    if (frame.isNull()) {
        qWarning() << "[ScreenStreamSender] Capture failed, skipping frame.";
        return;
    }

    // --- 3. Encode ---------------------------------------------------------
    QByteArray encoded = ScreenEncoder::encodeJpeg(frame);
    if (encoded.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Encode failed, skipping frame.";
        return;
    }

    // --- 4. Packetize ------------------------------------------------------
    const QList<QByteArray> packets =
        ScreenFramePacketizer::packetize(encoded, m_frameId);
    if (packets.isEmpty()) {
        qWarning() << "[ScreenStreamSender] Packetize returned empty, skipping frame.";
        return;
    }

    // --- 5. Send -----------------------------------------------------------
    for (const QByteArray &pkt : packets) {
        const qint64 written = m_relayClient->sendRawPacket(pkt);
        if (written < 0) {
            qWarning() << "[ScreenStreamSender] sendRawPacket failed (not connected?)."
                       << "Aborting frame" << m_frameId;
            return;   // do NOT increment frameId — frame was not fully sent
        }
    }

    // --- 6. Advance frameId ------------------------------------------------
    ++m_frameId;
}
