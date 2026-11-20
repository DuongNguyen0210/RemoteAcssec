#ifndef SCREENSTREAMSENDER_H
#define SCREENSTREAMSENDER_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
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
    static constexpr int MINOR_CHANGE_INTERVAL_MS = 400;
    static constexpr int KEEPALIVE_FRAME_INTERVAL_MS = 2500;
    static constexpr qint64 MAX_PENDING_BYTES = 256 * 1024LL;
    static constexpr qsizetype MAX_ENCODED_FRAME_BYTES = 256 * 1024;

    static FrameChange classifyFrameChange(const QImage &current, const QImage &previous);

    explicit ScreenStreamSender(ChildSessionController *session, QObject *parent = nullptr);
    ~ScreenStreamSender();

signals:
    void frameCaptured(const QSize &size);

private slots:
    void onTick();

private:
    QTimer      *m_timer;
    ChildSessionController *m_session;
    uint32_t     m_frameId;
    uint64_t     m_currentSessionId;
    ScreenCapture m_capture;
    QImage       m_lastSentFrame;
    EncodedScreenFrame m_lastEncodedFrame;
    QElapsedTimer m_lastSentTimer;
};

#endif
