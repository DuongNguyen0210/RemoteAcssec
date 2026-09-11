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

    qint64 sendRawPacket(const QByteArray &data);
    qint64 pendingBytes() const;

signals:
    void connected();
    void disconnected();
    void bytesReceived(const QByteArray &data);
    void transportError(const QString &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);
};

#endif
