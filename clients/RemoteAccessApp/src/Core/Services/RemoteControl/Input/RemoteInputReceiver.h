#pragma once

#include <QObject>
#include <QtGlobal>

#include "MouseInputInjector.h"
#include "Network/Protocol/RdtpStreamParser.h"

class RemoteInputReceiver : public QObject
{
    Q_OBJECT

public:
    explicit RemoteInputReceiver(QObject *parent = nullptr);

    void setActiveSession(quint64 sessionId);
    void handleMessage(const Protocol::RdtpStreamParser::Message &message);

private:
    quint64 m_activeSessionId;
    MouseInputInjector m_mouseInjector;
};
