#ifndef REMOTESCREENWIDGET_H
#define REMOTESCREENWIDGET_H

#include <QWidget>
#include <QImage>
#include <QElapsedTimer>

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

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_currentFrame;
    int m_frameCount = 0;
    int m_currentFps = 0;
    QElapsedTimer m_fpsTimer;
};

#endif // REMOTESCREENWIDGET_H
