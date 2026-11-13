#ifndef REMOTESCREENWIDGET_H
#define REMOTESCREENWIDGET_H

#include <QWidget>
#include <QImage>
#include <QElapsedTimer>
#include "Network/Protocol/ProtocolConstants.h"
#include <QSet>

class RemoteScreenWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RemoteScreenWidget(QWidget *parent = nullptr);
    ~RemoteScreenWidget() override = default;

    void updateFrame(const QImage &image);
    void clearFrame();

    QSize sizeHint() const override;
    int currentFps() const;
    QSize frameResolution() const;

signals:
    void fpsUpdated(int fps);
    void resolutionChanged(const QSize &size);
    void remoteInput(Protocol::MessageType type, const QByteArray &payload);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QImage m_currentFrame;
    int m_frameCount = 0;
    int m_currentFps = 0;
    QElapsedTimer m_fpsTimer;
    bool remotePosition(const QPoint &point, quint32 &x, quint32 &y) const;
    QSet<int> m_pressedKeys;
    QSet<quint8> m_pressedButtons;
    quint32 m_lastX = 0;
    quint32 m_lastY = 0;
};

#endif // REMOTESCREENWIDGET_H
