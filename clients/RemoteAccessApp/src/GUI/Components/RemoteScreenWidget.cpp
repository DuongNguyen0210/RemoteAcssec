#include "RemoteScreenWidget.h"
#include <QPainter>
#include <QPaintEvent>

RemoteScreenWidget::RemoteScreenWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_fpsTimer.start();
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
