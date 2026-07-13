#ifndef VIEWERWINDOW_H
#define VIEWERWINDOW_H

#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class QFrame;
class QTextEdit;

class QTcpSocket;

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

    QTcpSocket *socket;
    quint32 expectedImageSize;
    QByteArray imageBuffer;

private slots:
    void onConnectClicked();
    void onSocketConnected();
    void onSocketDisconnected();
    void onReadyRead();

};
#endif
