#ifndef SCREENSTREAMSENDER_H
#define SCREENSTREAMSENDER_H

#include <QObject>
#include <QTimer>
#include <cstdint>

#include "ScreenCapture.h"

class RelayClient;
class ChildSessionController;

class ScreenStreamSender : public QObject
{
    Q_OBJECT

public:
    static constexpr int STREAM_INTERVAL_MS = 200;
    static constexpr qint64 MAX_PENDING_BYTES = 2 * 524288LL;
    explicit ScreenStreamSender(ChildSessionController *session, QObject *parent = nullptr);
    ~ScreenStreamSender();

private slots:
    void onTick();

private:
    QTimer      *m_timer;
    RelayClient *m_relayClient;
    uint32_t     m_frameId;
    uint64_t     m_currentSessionId;
    ScreenCapture m_capture;
};

#endif
