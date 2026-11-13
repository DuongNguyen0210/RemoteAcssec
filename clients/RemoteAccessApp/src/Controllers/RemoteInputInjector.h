#pragma once
#include <QByteArray>
#include "Network/Protocol/ProtocolConstants.h"
#include <QSet>

class RemoteInputInjector {
public:
    bool inject(Protocol::MessageType type, const QByteArray &payload);
    void releaseAll();
private:
    QSet<quint32> m_keys;
    QSet<quint8> m_buttons;
};
