#pragma once

#include <QMainWindow>

class QImage;
class RemoteDesktopView;

class RemoteSessionWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RemoteSessionWindow(QWidget *parent = nullptr);

    void setRemoteFrame(const QImage &frame);

private:
    RemoteDesktopView *m_desktopView;
};
