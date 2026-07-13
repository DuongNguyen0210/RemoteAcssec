#ifndef HOSTSERVER_H
#define HOSTSERVER_H

#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;
class QTimer;

class HostServer : public QObject
{
    Q_OBJECT

public:
    explicit HostServer(QObject *parent = nullptr);

    bool start(quint16 port);

signals:
    void statusChanged(const QString &status);

private slots:
    void onNewConnection();
    void sendScreenFrame();

private:
    QTcpServer *server;
    QTcpSocket *clientSocket;
    QTimer *captureTimer;
};

#endif
