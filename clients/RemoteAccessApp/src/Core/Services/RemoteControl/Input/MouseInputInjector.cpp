#include "MouseInputInjector.h"

#include <QDebug>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

bool MouseInputInjector::inject(const RemoteMouseEvent &event) const
{
#ifdef Q_OS_WIN
    DWORD actionFlag = 0;
    DWORD mouseData = 0;

    switch (event.action) {
    case RemoteMouseAction::Move:
        break;
    case RemoteMouseAction::ButtonDown:
        switch (event.button) {
        case RemoteMouseButton::Left:
            actionFlag = MOUSEEVENTF_LEFTDOWN;
            break;
        case RemoteMouseButton::Right:
            actionFlag = MOUSEEVENTF_RIGHTDOWN;
            break;
        case RemoteMouseButton::Middle:
            actionFlag = MOUSEEVENTF_MIDDLEDOWN;
            break;
        case RemoteMouseButton::None:
            return false;
        }
        break;
    case RemoteMouseAction::ButtonUp:
        switch (event.button) {
        case RemoteMouseButton::Left:
            actionFlag = MOUSEEVENTF_LEFTUP;
            break;
        case RemoteMouseButton::Right:
            actionFlag = MOUSEEVENTF_RIGHTUP;
            break;
        case RemoteMouseButton::Middle:
            actionFlag = MOUSEEVENTF_MIDDLEUP;
            break;
        case RemoteMouseButton::None:
            return false;
        }
        break;
    case RemoteMouseAction::Wheel:
        actionFlag = MOUSEEVENTF_WHEEL;
        mouseData = static_cast<DWORD>(event.wheelDelta);
        break;
    }

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>(event.normalizedX);
    input.mi.dy = static_cast<LONG>(event.normalizedY);
    input.mi.mouseData = mouseData;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | actionFlag;

    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        qWarning() << "[MouseInputInjector] SendInput that bai, error="
                   << static_cast<qulonglong>(GetLastError());
        return false;
    }

    return true;
#else
    Q_UNUSED(event)
    static bool reportedUnsupportedPlatform = false;
    if (!reportedUnsupportedPlatform) {
        qWarning() << "[MouseInputInjector] Nen tang hien tai khong ho tro OS mouse injection.";
        reportedUnsupportedPlatform = true;
    }
    return false;
#endif
}
