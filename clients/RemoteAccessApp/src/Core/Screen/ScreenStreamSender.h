#ifndef SCREENSTSTREAMSENDER_H
#define SCREENSTSTREAMSENDER_H

#include <QObject>
#include <QTimer>
#include <cstdint>

class RelayClient;

// ---------------------------------------------------------------------------
// ScreenStreamSender
//
// GRASP: Controller / Pure Fabrication.
//
// Orchestrates the periodic CHILD→Relay screen streaming pipeline:
//
//   QTimer (200 ms)
//     → backpressure check via RelayClient::pendingBytes()
//     → ScreenCapture::capture()
//     → ScreenEncoder::encodeJpeg()
//     → ScreenFramePacketizer::packetize()
//     → RelayClient::sendRawPacket() for each RDTP packet
//
// Does NOT implement capture, encoding, packetization, or networking.
// Owns only: timer, frameId counter, RelayClient instance, backpressure policy.
// ---------------------------------------------------------------------------

class ScreenStreamSender : public QObject
{
    Q_OBJECT

public:
    // Interval between capture ticks (milliseconds).
    static constexpr int STREAM_INTERVAL_MS = 200;   // ≈ 5 FPS

    // Maximum bytes allowed in the relay socket output buffer before
    // the current frame is skipped.  Set to two raw maximum-size frames
    // to allow one frame in-flight while a second is being written.
    static constexpr qint64 MAX_PENDING_BYTES = 2 * 524288LL;   // 1 MB

    // Relay host / port — must match the running Relay Proxy.
    static constexpr quint16 RELAY_PORT = 8080;

    explicit ScreenStreamSender(QObject *parent = nullptr);
    ~ScreenStreamSender();

    // Connect to the relay and start the 200 ms timer.
    void start();

    // Stop the timer.
    void stop();

private slots:
    void onTick();

private:
    QTimer      *m_timer;
    RelayClient *m_relayClient;
    uint32_t     m_frameId;
};

#endif // SCREENSTSTREAMSENDER_H
