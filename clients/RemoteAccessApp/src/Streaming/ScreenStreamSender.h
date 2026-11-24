#ifndef SCREENSTREAMSENDER_H
#define SCREENSTREAMSENDER_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QImage>
#include <cstdint>

#include "ScreenCapture.h"
#include "ScreenEncoder.h"

class ChildSessionController;

class ScreenStreamSender : public QObject
{
    Q_OBJECT

public:
    enum class FrameChange {
        None,
        Minor,
        Significant
    };

    static constexpr int STREAM_INTERVAL_MS = 100;
    static constexpr int MINOR_CHANGE_INTERVAL_MS = 250;
    static constexpr int REFRESH_INTERVAL_MS = 1200;
    static constexpr qsizetype MAX_WIRE_FRAME_BYTES = 256 * 1024;

    static FrameChange classifyFrameChange(const QImage &current, const QImage &previous);

    explicit ScreenStreamSender(ChildSessionController *session, QObject *parent = nullptr);
    ~ScreenStreamSender();

signals:
    void frameCaptured(const QSize &size);

private slots:
    void onTick();
    void sendEncodedFrame();

private:
    QTimer      *m_timer;
    ChildSessionController *m_session;
    uint32_t     m_frameId;
    uint64_t     m_currentSessionId;
    ScreenCapture m_capture;
    QImage       m_lastSentFrame;
    QImage       m_encodingFrame;
    quint64      m_encodingSessionId = 0;
    QFutureWatcher<EncodedScreenFrame> m_encoder;
    QElapsedTimer m_lastSentTimer;
    qint64 m_nextCaptureDelayMs = 0;
    const int m_wireBytesPerSecond;
};

#endif
