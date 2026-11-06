#include "ScreenCapture.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QDebug>

QImage ScreenCapture::capture() const
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qWarning() << "[ScreenCapture] Không tìm thấy màn hình chính.";
        return QImage{};
    }

    const QRect geom = screen->geometry();
    QPixmap pixmap = screen->grabWindow(0, geom.x(), geom.y(), geom.width(), geom.height());

    if (pixmap.isNull()) {
        pixmap = screen->grabWindow(0);
    }

    if (pixmap.isNull()) {
        qWarning() << "[ScreenCapture] Chụp màn hình thất bại.";
        return QImage{};
    }

    return pixmap.toImage();
}
