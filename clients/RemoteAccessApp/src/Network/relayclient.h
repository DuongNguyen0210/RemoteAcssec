#ifndef RELAYCLIENT_H
#define RELAYCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QString>

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
    void sendRegisterHostRequest(uint64_t mySessionId);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
};

#endif
