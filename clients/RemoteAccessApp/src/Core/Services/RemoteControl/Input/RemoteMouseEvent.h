#pragma once

#include <QMetaType>
#include <QtGlobal>

enum class RemoteMouseAction
{
    Move,
    ButtonDown,
    ButtonUp,
    Wheel
};

enum class RemoteMouseButton
{
    None = 0,
    Left = 1,
    Right = 2,
    Middle = 3
};

struct RemoteMouseEvent
{
    RemoteMouseAction action = RemoteMouseAction::Move;
    quint16 normalizedX = 0;
    quint16 normalizedY = 0;
    RemoteMouseButton button = RemoteMouseButton::None;
    qint32 wheelDelta = 0;
};

Q_DECLARE_METATYPE(RemoteMouseEvent)
