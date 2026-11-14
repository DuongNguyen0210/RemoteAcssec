#pragma once
#include <QByteArray>
#include "Network/Protocol/ProtocolConstants.h"
#include <QSet>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <optional>

class RemoteInputInjector {
public:
    bool inject(Protocol::MessageType type, const QByteArray &payload);
    void releaseAll();
    void setFrameSize(const QSize &size) { m_frameSize = size; }
    static std::optional<QPoint> desktopPosition(quint32 x, quint32 y,
                                                  const QSize &frameSize, const QRect &screenGeometry);
private:
    QSet<quint32> m_keys;
    QSet<quint8> m_buttons;
    QSize m_frameSize;
    QPoint m_lastRemotePosition;
};
