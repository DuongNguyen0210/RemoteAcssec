#ifndef RELAYCLIENT_H
#define RELAYCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QByteArray>

class RelayClient : public QObject
{
    Q_OBJECT
private:
    QTcpSocket* m_socket;

public:
    explicit RelayClient(QObject *parent = nullptr);
    ~RelayClient();

    void ConnectToServer(const QString& host, quint16 port);
    void DisconnectFromServer();

    // Write a pre-built RDTP packet to the relay socket.
    // Returns bytes written (>= 0) on success, or -1 if not connected or data empty.
    qint64 sendRawPacket(const QByteArray &data);

    // Returns QTcpSocket::bytesToWrite() — bytes queued but not yet sent.
    // Used by ScreenStreamSender for backpressure decisions.
    qint64 pendingBytes() const;

signals:
    void connected();
    void disconnected();
    void bytesReceived(const QByteArray &data);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
};

#endif
