#include "RemoteDesktopView.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QWheelEvent>
#include <QtMath>

#include <optional>

namespace {

QPointF eventPosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}

QPointF eventPosition(const QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->posF();
#endif
}

std::optional<RemoteMouseButton> remoteButton(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton:
        return RemoteMouseButton::Left;
    case Qt::RightButton:
        return RemoteMouseButton::Right;
    case Qt::MiddleButton:
        return RemoteMouseButton::Middle;
    default:
        return std::nullopt;
    }
}

}

RemoteDesktopView::RemoteDesktopView(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
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

void RemoteDesktopView::mouseMoveEvent(QMouseEvent *event)
{
    RemoteMouseEvent remoteEvent;
    remoteEvent.action = RemoteMouseAction::Move;
    if (!normalizedPosition(eventPosition(event),
                            remoteEvent.normalizedX,
                            remoteEvent.normalizedY)) {
        event->ignore();
        return;
    }

    emit remoteMouseInput(remoteEvent);
    event->accept();
}

void RemoteDesktopView::mousePressEvent(QMouseEvent *event)
{
    const auto button = remoteButton(event->button());
    if (!button.has_value()) {
        event->ignore();
        return;
    }

    RemoteMouseEvent remoteEvent;
    remoteEvent.action = RemoteMouseAction::ButtonDown;
    remoteEvent.button = button.value();
    if (!normalizedPosition(eventPosition(event),
                            remoteEvent.normalizedX,
                            remoteEvent.normalizedY)) {
        event->ignore();
        return;
    }

    setFocus(Qt::MouseFocusReason);
    emit remoteMouseInput(remoteEvent);
    event->accept();
}

void RemoteDesktopView::mouseReleaseEvent(QMouseEvent *event)
{
    const auto button = remoteButton(event->button());
    if (!button.has_value()) {
        event->ignore();
        return;
    }

    RemoteMouseEvent remoteEvent;
    remoteEvent.action = RemoteMouseAction::ButtonUp;
    remoteEvent.button = button.value();
    if (!normalizedPosition(eventPosition(event),
                            remoteEvent.normalizedX,
                            remoteEvent.normalizedY)) {
        event->ignore();
        return;
    }

    emit remoteMouseInput(remoteEvent);
    event->accept();
}

void RemoteDesktopView::wheelEvent(QWheelEvent *event)
{
    const int verticalDelta = event->angleDelta().y();
    if (verticalDelta == 0) {
        event->ignore();
        return;
    }

    RemoteMouseEvent remoteEvent;
    remoteEvent.action = RemoteMouseAction::Wheel;
    remoteEvent.wheelDelta = static_cast<qint32>(verticalDelta);
    if (!normalizedPosition(eventPosition(event),
                            remoteEvent.normalizedX,
                            remoteEvent.normalizedY)) {
        event->ignore();
        return;
    }

    emit remoteMouseInput(remoteEvent);
    event->accept();
}

QRect RemoteDesktopView::displayedImageRect() const
{
    if (m_latestFrame.isNull())
        return {};

    const QSize renderedSize = m_latestFrame.size().scaled(
            size(), Qt::KeepAspectRatio);
    return QRect(
            QPoint((width() - renderedSize.width()) / 2,
                   (height() - renderedSize.height()) / 2),
            renderedSize);
}

bool RemoteDesktopView::normalizedPosition(const QPointF &position,
                                           quint16 &normalizedX,
                                           quint16 &normalizedY) const
{
    const QRect imageRect = displayedImageRect();
    if (imageRect.isEmpty()
            || !imageRect.contains(QPoint(qFloor(position.x()),
                                         qFloor(position.y())))) {
        return false;
    }

    const auto normalize = [](qreal coordinate, int origin, int extent) {
        if (extent <= 1)
            return static_cast<quint16>(0);

        const qreal local = qBound<qreal>(
                0.0, coordinate - origin, static_cast<qreal>(extent - 1));
        return static_cast<quint16>(qRound(
                local * 65535.0 / static_cast<qreal>(extent - 1)));
    };

    normalizedX = normalize(position.x(), imageRect.left(), imageRect.width());
    normalizedY = normalize(position.y(), imageRect.top(), imageRect.height());
    return true;
}
