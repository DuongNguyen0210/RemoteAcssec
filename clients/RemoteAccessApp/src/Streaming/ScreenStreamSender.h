#ifndef SCREENSTREAMSENDER_H
#define SCREENSTREAMSENDER_H

#include <QObject>
#include <QTimer>
#include <cstdint>

#include "ScreenCapture.h"

class ChildSessionController;

class ScreenStreamSender : public QObject
{
    Q_OBJECT

public:
    static constexpr int STREAM_INTERVAL_MS = 200;
    static constexpr qint64 MAX_PENDING_BYTES = 2 * 1024 * 1024LL;
    static constexpr qsizetype MAX_ENCODED_FRAME_BYTES = 1024 * 1024;

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
};

#endif
