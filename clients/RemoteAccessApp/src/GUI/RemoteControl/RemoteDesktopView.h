#pragma once

#include <QImage>
#include <QWidget>

class RemoteDesktopView : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteDesktopView(QWidget *parent = nullptr);

    void setRemoteFrame(const QImage &frame);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_latestFrame;
};
