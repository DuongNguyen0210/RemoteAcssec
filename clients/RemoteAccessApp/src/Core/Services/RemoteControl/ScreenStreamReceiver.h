#pragma once

#include <QImage>
#include <QObject>
#include <QtGlobal>

#include "Network/Protocol/RdtpStreamParser.h"
#include "ScreenFrameReassembler.h"

class ScreenStreamReceiver : public QObject
{
    Q_OBJECT

public:
    explicit ScreenStreamReceiver(QObject *parent = nullptr);

    void setActiveSession(quint64 sessionId);
    void handleMessage(const Protocol::RdtpStreamParser::Message &message);

signals:
    void frameReady(const QImage &image);

private:
    quint64 m_activeSessionId;
    ScreenFrameReassembler m_reassembler;
};
