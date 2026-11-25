#ifndef SCREENSTREAMSENDER_H
#define SCREENSTREAMSENDER_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QImage>
#include <cstdint>
#include <optional>

#include "ScreenCapture.h"
#include "ScreenEncoder.h"
#include "ScreenStreamRate.h"

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

    static constexpr int STREAM_INTERVAL_MS = ScreenStreamRate::MinSendIntervalMs;
    static constexpr int MINOR_CHANGE_INTERVAL_MS = 250;
    static constexpr int REFRESH_INTERVAL_MS = 1200;
    static constexpr qsizetype MAX_WIRE_FRAME_BYTES = ScreenStreamRate::MaxWireFrameBytes;

    static FrameChange classifyFrameChange(const QImage &current, const QImage &previous);

    explicit ScreenStreamSender(ChildSessionController *session, QObject *parent = nullptr);
    ~ScreenStreamSender();

signals:
    void frameCaptured(const QSize &size);

private slots:
    void onTick();
    void encodingFinished();

private:
    void resetSession(quint64 sessionId);
    void sendPreparedFrame();
    void schedule(qint64 delayMs);

    QTimer      *m_timer;
    ChildSessionController *m_session;
    uint32_t     m_frameId;
    uint64_t     m_currentSessionId;
    ScreenCapture m_capture;
    QImage       m_lastSentFrame;
    QImage       m_encodingFrame;
    quint64      m_encodingSessionId = 0;
    QFutureWatcher<EncodedScreenFrame> m_encoder;
    ScreenStreamRate m_rate;
    ScreenEncodeState m_search;
    std::optional<EncodedScreenFrame> m_prepared;
    QElapsedTimer m_clock;
    qint64 m_captureStartMs = 0;
    qint64 m_workMs = 0;
    bool m_workerActive = false;
};

#endif
