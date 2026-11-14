#include "RemoteInputInjector.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QtEndian>
#include <utility>
#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <dlfcn.h>
#endif

namespace {
unsigned long nativeKey(quint32 key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return key;
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return key;
    if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde) return key;
    switch (key) {
    case Qt::Key_Return: case Qt::Key_Enter: return 0xff0d;
    case Qt::Key_Backspace: return 0xff08;
    case Qt::Key_Tab: return 0xff09;
    case Qt::Key_Escape: return 0xff1b;
    case Qt::Key_Delete: return 0xffff;
    case Qt::Key_Left: return 0xff51;
    case Qt::Key_Up: return 0xff52;
    case Qt::Key_Right: return 0xff53;
    case Qt::Key_Down: return 0xff54;
    case Qt::Key_Shift: return 0xffe1;
    case Qt::Key_Control: return 0xffe3;
    case Qt::Key_Alt: return 0xffe9;
    default: return 0;
    }
}

bool movePointer(const QByteArray &payload, const QSize &frameSize, QPoint &lastRemotePosition) {
    quint32 x, y;
    if (!RemoteInputPacket::readPosition(payload, x, y)) return false;
    auto *screen = QGuiApplication::primaryScreen();
    if (!screen) return false;
    const auto position = RemoteInputInjector::desktopPosition(x, y, frameSize, screen->geometry());
    if (!position) return false;
    QCursor::setPos(*position);
    lastRemotePosition = QPoint(int(x), int(y));
    return true;
}
}

std::optional<QPoint> RemoteInputInjector::desktopPosition(quint32 x, quint32 y,
                                                            const QSize &frameSize, const QRect &screenGeometry) {
    if (frameSize.isEmpty() || screenGeometry.isEmpty()
            || x >= quint32(frameSize.width()) || y >= quint32(frameSize.height())) return std::nullopt;
    return QPoint(screenGeometry.x() + int(quint64(x) * screenGeometry.width() / frameSize.width()),
                  screenGeometry.y() + int(quint64(y) * screenGeometry.height() / frameSize.height()));
}

bool RemoteInputInjector::inject(Protocol::MessageType type, const QByteArray &payload) {
    using T = Protocol::MessageType;
#ifdef Q_OS_LINUX
    if (QGuiApplication::platformName() != "xcb") {
        static bool warned = false;
        if (!warned) {
            qWarning() << "Remote input requires an X11 (xcb) CHILD session; Wayland is unsupported.";
            warned = true;
        }
        return false;
    }
#endif
    if (type == T::MOUSE_MOVE) return payload.size() == 8 && movePointer(payload, m_frameSize, m_lastRemotePosition);
    if (type == T::MOUSE_BUTTON_DOWN || type == T::MOUSE_BUTTON_UP || type == T::MOUSE_WHEEL) {
        if (payload.size() != (type == T::MOUSE_WHEEL ? 12 : 9)
                || !movePointer(payload, m_frameSize, m_lastRemotePosition)) return false;
    } else if (type == T::KEY_PRESS || type == T::KEY_RELEASE) {
        if (payload.size() != 4) return false;
    } else return false;

#ifdef Q_OS_WIN
    INPUT input{};
    if (type == T::MOUSE_BUTTON_DOWN || type == T::MOUSE_BUTTON_UP) {
        const bool down = type == T::MOUSE_BUTTON_DOWN;
        const auto id = quint8(payload.at(8));
        const DWORD flags[] = {0, down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP,
            down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP,
            down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP};
        if (id < 1 || id > 3) return false;
        input.type = INPUT_MOUSE; input.mi.dwFlags = flags[id];
    } else if (type == T::MOUSE_WHEEL) {
        input.type = INPUT_MOUSE; input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(payload.constData() + 8));
    } else {
        const auto key = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(payload.constData()));
        unsigned long native = nativeKey(key);
        if (native >= 32 && native <= 126 && !(native >= 'A' && native <= 'Z')
                && !(native >= '0' && native <= '9')) {
            const SHORT mapped = VkKeyScanW(wchar_t(native));
            if (mapped == -1) return false;
            native = LOBYTE(mapped);
        } else if (native >= 0xff00) {
            switch (key) {
            case Qt::Key_Return: case Qt::Key_Enter: native = VK_RETURN; break;
            case Qt::Key_Backspace: native = VK_BACK; break;
            case Qt::Key_Tab: native = VK_TAB; break;
            case Qt::Key_Escape: native = VK_ESCAPE; break;
            case Qt::Key_Delete: native = VK_DELETE; break;
            case Qt::Key_Left: native = VK_LEFT; break;
            case Qt::Key_Up: native = VK_UP; break;
            case Qt::Key_Right: native = VK_RIGHT; break;
            case Qt::Key_Down: native = VK_DOWN; break;
            case Qt::Key_Shift: native = VK_SHIFT; break;
            case Qt::Key_Control: native = VK_CONTROL; break;
            case Qt::Key_Alt: native = VK_MENU; break;
            default: return false;
            }
        }
        if (!native) return false;
        input.type = INPUT_KEYBOARD; input.ki.wVk = WORD(native);
        if (type == T::KEY_RELEASE) input.ki.dwFlags = KEYEVENTF_KEYUP;
    }
    const bool sent = SendInput(1, &input, sizeof(input)) == 1;
#elif defined(Q_OS_LINUX)
    auto *display = XOpenDisplay(nullptr);
    if (!display) return false;
    void *library = dlopen("libXtst.so.6", RTLD_NOW);
    if (!library) { XCloseDisplay(display); return false; }
    using Button = int (*)(Display *, unsigned int, int, unsigned long);
    using Key = int (*)(Display *, unsigned int, int, unsigned long);
    auto button = reinterpret_cast<Button>(dlsym(library, "XTestFakeButtonEvent"));
    auto keyEvent = reinterpret_cast<Key>(dlsym(library, "XTestFakeKeyEvent"));
    bool sent = false;
    if (button && keyEvent) {
        if (type == T::MOUSE_BUTTON_DOWN || type == T::MOUSE_BUTTON_UP) {
            const int id = quint8(payload.at(8));
            const int mapped = id == 1 ? 1 : id == 2 ? 3 : id == 3 ? 2 : 0;
            sent = mapped && button(display, mapped, type == T::MOUSE_BUTTON_DOWN, 0);
        } else if (type == T::MOUSE_WHEEL) {
            const auto delta = qint32(qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(payload.constData() + 8)));
            const unsigned int wheelButton = delta > 0 ? 4 : 5;
            for (int i = 0; i < qMin(10, qAbs(delta) / 120); ++i) {
                sent = button(display, wheelButton, 1, 0) && button(display, wheelButton, 0, 0);
            }
        } else {
            const auto qtKey = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(payload.constData()));
            const auto code = XKeysymToKeycode(display, nativeKey(qtKey));
            sent = code && keyEvent(display, code, type == T::KEY_PRESS, 0);
        }
        XFlush(display);
    }
    // XCloseDisplay invokes extension cleanup callbacks owned by libXtst.
    // Keep the library loaded until those callbacks have finished.
    XCloseDisplay(display);
    dlclose(library);
#else
    const bool sent = false;
#endif
    if (sent && (type == T::KEY_PRESS || type == T::KEY_RELEASE)) {
        const auto key = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(payload.constData()));
        if (type == T::KEY_PRESS) m_keys.insert(key); else m_keys.remove(key);
    }
    if (sent && (type == T::MOUSE_BUTTON_DOWN || type == T::MOUSE_BUTTON_UP)) {
        const quint8 id = quint8(payload.at(8));
        if (type == T::MOUSE_BUTTON_DOWN) m_buttons.insert(id); else m_buttons.remove(id);
    }
    return sent;
}

void RemoteInputInjector::releaseAll() {
    const auto keys = m_keys;
    for (quint32 key : keys) inject(Protocol::MessageType::KEY_RELEASE, RemoteInputPacket::key(key));
    const auto buttons = m_buttons;
    if (!m_frameSize.isEmpty()) for (quint8 id : buttons)
        inject(Protocol::MessageType::MOUSE_BUTTON_UP,
               RemoteInputPacket::button(quint32(m_lastRemotePosition.x()),
                                         quint32(m_lastRemotePosition.y()), id));
    m_keys.clear(); m_buttons.clear();
    m_frameSize = {};
}
