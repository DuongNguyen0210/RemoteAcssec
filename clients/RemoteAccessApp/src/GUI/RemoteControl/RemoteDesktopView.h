#pragma once

#include <QImage>
#include <QPointF>
#include <QRect>
#include <QWidget>

#include "Core/Services/RemoteControl/Input/RemoteMouseEvent.h"

class QMouseEvent;
class QWheelEvent;

class RemoteDesktopView : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteDesktopView(QWidget *parent = nullptr);

    void setRemoteFrame(const QImage &frame);
    QSize sizeHint() const override;

signals:
    void remoteMouseInput(const RemoteMouseEvent &event);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QRect displayedImageRect() const;
    bool normalizedPosition(const QPointF &position,
                            quint16 &normalizedX,
                            quint16 &normalizedY) const;

    QImage m_latestFrame;
};
