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

    QPixmap pixmap = screen->grabWindow(0);

    if (pixmap.isNull()) {
        qWarning() << "[ScreenCapture] grabWindow(0) returned a null pixmap.";
        return QImage{};
    }

    QImage image = pixmap.toImage();

    qDebug() << "[ScreenCapture] captured"
             << "width="  << image.width()
             << "height=" << image.height()
             << "format=" << image.format();

    return image;
}
