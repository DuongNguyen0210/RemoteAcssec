#include "ScreenCapture.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QDebug>

QImage ScreenCapture::capture() const
{
    QScreen *screen = QGuiApplication::primaryScreen();

    if (!screen) {
        qWarning() << "[ScreenCapture] No primary screen available.";
        return QImage{};
    }

    const QRect geom = screen->geometry();
    QPixmap pixmap = screen->grabWindow(0, geom.x(), geom.y(), geom.width(), geom.height());

    if (pixmap.isNull()) {
        pixmap = screen->grabWindow(0);
    }

    if (pixmap.isNull()) {
        qWarning() << "[ScreenCapture] grabWindow(0) returned a null pixmap."
#if defined(Q_OS_LINUX)
                   << "On Linux Wayland, ensure XWayland is available (DISPLAY is set).";
#elif defined(Q_OS_WIN)
                   << "On Windows, ensure the session is not locked.";
#endif
        return QImage{};
    }

    QImage image = pixmap.toImage();

    qDebug() << "[ScreenCapture] captured"
             << "width="  << image.width()
             << "height=" << image.height()
             << "format=" << image.format();

    return image;
}
