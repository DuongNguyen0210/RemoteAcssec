#include "RemoteScreenWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include "Network/Protocol/RemoteInputPacket.h"
#include <utility>

RemoteScreenWidget::RemoteScreenWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    m_fpsTimer.start();
}

bool RemoteScreenWidget::remotePosition(const QPoint &point, quint32 &x, quint32 &y) const {
    if (m_currentFrame.isNull()) return false;
    const QSize drawn = m_currentFrame.size().scaled(size(), Qt::KeepAspectRatio);
    if (drawn.isEmpty()) return false;
    const QRect target((width() - drawn.width()) / 2, (height() - drawn.height()) / 2,
                       drawn.width(), drawn.height());
    if (!target.contains(point)) return false;
    x = quint32(qBound(0, (point.x() - target.left()) * m_currentFrame.width() / drawn.width(), m_currentFrame.width() - 1));
    y = quint32(qBound(0, (point.y() - target.top()) * m_currentFrame.height() / drawn.height(), m_currentFrame.height() - 1));
    return true;
}

void RemoteScreenWidget::mouseMoveEvent(QMouseEvent *event) {
    quint32 x, y;
    if (remotePosition(event->pos(), x, y)) {
        m_lastX = x; m_lastY = y;
        emit remoteInput(Protocol::MessageType::MOUSE_MOVE, RemoteInputPacket::position(x, y));
    }
}

void RemoteScreenWidget::mousePressEvent(QMouseEvent *event) {
    setFocus();
    quint32 x, y;
    const quint8 button = event->button() == Qt::LeftButton ? 1 : event->button() == Qt::RightButton ? 2 : event->button() == Qt::MiddleButton ? 3 : 0;
    if (button && remotePosition(event->pos(), x, y)) {
        m_lastX = x; m_lastY = y;
        m_pressedButtons.insert(button);
        emit remoteInput(Protocol::MessageType::MOUSE_BUTTON_DOWN, RemoteInputPacket::button(x, y, button));
    }
}

void RemoteScreenWidget::mouseReleaseEvent(QMouseEvent *event) {
    quint32 x, y;
    const quint8 button = event->button() == Qt::LeftButton ? 1 : event->button() == Qt::RightButton ? 2 : event->button() == Qt::MiddleButton ? 3 : 0;
    if (!button || !m_pressedButtons.remove(button)) return;
    if (remotePosition(event->pos(), x, y)) { m_lastX = x; m_lastY = y; }
    emit remoteInput(Protocol::MessageType::MOUSE_BUTTON_UP, RemoteInputPacket::button(m_lastX, m_lastY, button));
}

void RemoteScreenWidget::wheelEvent(QWheelEvent *event) {
    quint32 x, y;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint point = event->position().toPoint();
#else
    const QPoint point = event->pos();
#endif
    if (remotePosition(point, x, y)) emit remoteInput(Protocol::MessageType::MOUSE_WHEEL, RemoteInputPacket::wheel(x, y, event->angleDelta().y()));
}

void RemoteScreenWidget::keyPressEvent(QKeyEvent *event) {
    if (!m_currentFrame.isNull() && !event->isAutoRepeat()) {
        m_pressedKeys.insert(event->key());
        emit remoteInput(Protocol::MessageType::KEY_PRESS, RemoteInputPacket::key(quint32(event->key())));
    }
}

void RemoteScreenWidget::keyReleaseEvent(QKeyEvent *event) {
    if (!event->isAutoRepeat() && m_pressedKeys.remove(event->key()))
        emit remoteInput(Protocol::MessageType::KEY_RELEASE, RemoteInputPacket::key(quint32(event->key())));
}

void RemoteScreenWidget::focusOutEvent(QFocusEvent *event) {
    for (quint8 button : std::as_const(m_pressedButtons))
        emit remoteInput(Protocol::MessageType::MOUSE_BUTTON_UP, RemoteInputPacket::button(m_lastX, m_lastY, button));
    m_pressedButtons.clear();
    for (int key : std::as_const(m_pressedKeys))
        emit remoteInput(Protocol::MessageType::KEY_RELEASE, RemoteInputPacket::key(quint32(key)));
    m_pressedKeys.clear();
    QWidget::focusOutEvent(event);
}

void RemoteScreenWidget::updateFrame(const QImage &image)
{
    if (image.isNull())
        return;

    const bool resChanged = (m_currentFrame.size() != image.size());
    m_currentFrame = image;

    m_frameCount++;
    const qint64 elapsedMs = m_fpsTimer.elapsed();
    if (elapsedMs >= 1000) {
        m_currentFps = static_cast<int>((m_frameCount * 1000) / elapsedMs);
        m_frameCount = 0;
        m_fpsTimer.restart();
        emit fpsUpdated(m_currentFps);
    }

    if (resChanged) {
        emit resolutionChanged(m_currentFrame.size());
    }

    update();
}

void RemoteScreenWidget::clearFrame()
{
    m_currentFrame = QImage();
    m_frameCount = 0;
    m_currentFps = 0;
    m_fpsTimer.restart();
    emit fpsUpdated(0);
    update();
}

QSize RemoteScreenWidget::sizeHint() const
{
    return QSize(960, 540);
}

int RemoteScreenWidget::currentFps() const
{
    return m_currentFps;
}

QSize RemoteScreenWidget::frameResolution() const
{
    return m_currentFrame.size();
}

void RemoteScreenWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(18, 20, 22));

    if (m_currentFrame.isNull()) {
        painter.setPen(QColor(160, 168, 175));
        QFont font = painter.font();
        font.setPointSize(14);
        font.setWeight(QFont::Medium);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Đang chờ tín hiệu màn hình từ máy từ xa..."));
        return;
    }

    const QSize targetSize = m_currentFrame.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect targetRect(
        (width() - targetSize.width()) / 2,
        (height() - targetSize.height()) / 2,
        targetSize.width(),
        targetSize.height()
    );

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(targetRect, m_currentFrame);
}
