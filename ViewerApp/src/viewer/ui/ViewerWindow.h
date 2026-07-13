#ifndef VIEWERWINDOW_H
#define VIEWERWINDOW_H

#include <QByteArray>
#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class QFrame;
class QTextEdit;
class ViewerClient;

class ViewerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit  ViewerWindow(QWidget *parent = nullptr);
    ~ViewerWindow();

private:
    QLineEdit *partnerInput;
    QLineEdit *partnerPort;
    QPushButton *bttConnect;
    QLabel *statusLabel;
    QFrame *screenFrame;
    QTextEdit *logText;
    QLabel *screenLabel;
    ViewerClient *client;

private slots:
    void onConnectClicked();
    void onSocketConnected();
    void onSocketDisconnected();
    void onFrameReceived(const QByteArray &imageBuffer);
};

#endif
