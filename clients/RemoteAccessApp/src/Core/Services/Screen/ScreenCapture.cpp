#include "ScreenCapture.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QDebug>

// ---------------------------------------------------------------------------
// capture()
//
// Implementation notes:
//   - QGuiApplication::primaryScreen() retrieves the primary display.
//     Returns nullptr if no QGuiApplication exists yet (guard included).
//   - QScreen::grabWindow(0) captures the entire desktop window (WId = 0).
//     This is the standard Qt approach for full-screen capture.
//   - QPixmap::toImage() converts the capture to a device-independent
//     QImage so callers do not depend on display-specific QPixmap internals.
//   - All failure paths return a null QImage without crashing or looping.
// ---------------------------------------------------------------------------

QImage ScreenCapture::capture() const
{
    QScreen *screen = QGuiApplication::primaryScreen();

    if (!screen) {
        qWarning() << "[ScreenCapture] No primary screen available.";
        return QImage{};   // null image
    }

    QPixmap pixmap = screen->grabWindow(0);

    if (pixmap.isNull()) {
        qWarning() << "[ScreenCapture] grabWindow(0) returned a null pixmap.";
        return QImage{};   // null image
    }

    QImage image = pixmap.toImage();

    qDebug() << "[ScreenCapture] captured"
             << "width="  << image.width()
             << "height=" << image.height()
             << "format=" << image.format();

    return image;
}
