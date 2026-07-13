#ifndef HOSTWINDOW_H
#define HOSTWINDOW_H

#include <QWidget>

class QPushButton;
class QLabel;

class QTcpServer;
class QTcpSocket;
class QTimer;

class HostWindow : public QWidget
{
    Q_OBJECT

public:
    explicit HostWindow(QWidget *parent = nullptr);
    ~HostWindow();

private:
    QPushButton *startButton;
    QLabel *statusLabel;

    QTcpServer *server;
    QTcpSocket *clientSocket;
    QTimer *captureTimer;

private slots:
    void onStartSharingClicked();
    void onNewConnection();
    void sendScreenFrame();
};
#endif
