#include "RemoteDesktopView.h"

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>

RemoteDesktopView::RemoteDesktopView(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(320, 180);
}

void RemoteDesktopView::setRemoteFrame(const QImage &frame)
{
    if (frame.isNull())
        return;

    m_latestFrame = frame;
    update();
}

QSize RemoteDesktopView::sizeHint() const
{
    return QSize(960, 540);
}

void RemoteDesktopView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (m_latestFrame.isNull()) {
        painter.setPen(Qt::lightGray);
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("Đang chờ khung hình..."));
        return;
    }

    const QSize renderedSize = m_latestFrame.size().scaled(
            size(), Qt::KeepAspectRatio);
    const QRect targetRect(
            QPoint((width() - renderedSize.width()) / 2,
                   (height() - renderedSize.height()) / 2),
            renderedSize);

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(targetRect, m_latestFrame);
}
