#ifndef HOSTWINDOW_H
#define HOSTWINDOW_H

#include <QWidget>

class QPushButton;
class QLabel;
class HostServer;

class HostWindow : public QWidget
{
    Q_OBJECT

public:
    explicit HostWindow(QWidget *parent = nullptr);
    ~HostWindow();

private:
    QPushButton *startButton;
    QLabel *statusLabel;
    HostServer *server;

private slots:
    void onStartSharingClicked();
};

#endif
