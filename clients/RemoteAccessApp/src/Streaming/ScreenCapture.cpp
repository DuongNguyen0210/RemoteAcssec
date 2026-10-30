#include "ScreenCapture.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QDebug>
#include <QPainter>
#include <QDateTime>
#include <algorithm>

namespace {
#if defined(Q_OS_LINUX)
bool isAllBlack(const QImage &img)
{
    if (img.isNull()) return true;
    const int w = img.width();
    const int h = img.height();
    for (int i = 1; i <= 5; ++i) {
        for (int j = 1; j <= 6; ++j) {
            const QRgb p = img.pixel((w * i) / 6, (h * j) / 7);
            if ((p & 0x00FFFFFF) != 0) {
                return false;
            }
        }
    }
    return true;
}

void drawWaylandNotice(QImage &img)
{
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);

    const int cardW = std::min(720, img.width() - 40);
    const int cardH = 260;
    const QRect cardRect((img.width() - cardW) / 2, (img.height() - cardH) / 2, cardW, cardH);

    painter.fillRect(cardRect, QColor(24, 26, 30, 235));
    painter.setPen(QColor(60, 70, 80));
    painter.drawRect(cardRect);

    // Title
    painter.setPen(QColor(255, 255, 255));
    QFont font = painter.font();
    font.setPointSize(15);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(cardRect.adjusted(20, 18, -20, -185), Qt::AlignCenter,
                     QStringLiteral("⚠️ Thông báo tương thích Linux Wayland"));

    // Body
    painter.setPen(QColor(210, 215, 220));
    font.setPointSize(11);
    font.setBold(false);
    painter.setFont(font);
    const QString notice = QStringLiteral(
        "Luồng truyền màn hình (Live Stream) đang hoạt động thời gian thực!\n\n"
        "Tuy nhiên trên Linux Wayland, cơ chế bảo mật chặn đọc nội dung các cửa sổ khác.\n"
        "👉 Để truyền màn hình thật: Vui lòng 'Log out' và chọn 'Ubuntu on Xorg' khi đăng nhập,\n"
        "hoặc chạy ứng dụng trên hệ điều hành Windows."
    );
    painter.drawText(cardRect.adjusted(20, 55, -20, -55), Qt::AlignCenter, notice);

    // Live clock
    painter.setPen(QColor(70, 180, 255));
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);
    const QString timeStr = QStringLiteral("⏱️ Live Clock: %1")
                                .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"));
    painter.drawText(cardRect.adjusted(20, 215, -20, -15), Qt::AlignCenter, timeStr);
}
#endif
} // namespace

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

#if defined(Q_OS_LINUX)
    static bool warnedWayland = false;
    if (isAllBlack(image)) {
        if (!warnedWayland) {
            warnedWayland = true;
            qWarning() << "[ScreenCapture] Man hinh chup bi den do co che bao mat Linux Wayland."
                       << "Hay dang nhap bang 'Ubuntu on Xorg' de chup man hinh that hoac chay tren Windows.";
        }
        drawWaylandNotice(image);
    }
#endif

    qDebug() << "[ScreenCapture] captured"
             << "width="  << image.width()
             << "height=" << image.height()
             << "format=" << image.format();

    return image;
}
