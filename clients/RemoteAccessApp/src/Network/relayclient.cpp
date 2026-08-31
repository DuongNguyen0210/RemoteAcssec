#include "relayclient.h"
#include <QDebug>

RelayClient::RelayClient(QObject *parent) : QObject(parent) {
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &RelayClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &RelayClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &RelayClient::onReadyRead);
}

RelayClient::~RelayClient() {
}

void RelayClient::ConnectToServer(const QString& host, quint16 port)
{
    m_socket->connectToHost(host, port);
}

void RelayClient::DisconnectFromServer()
{
    m_socket->disconnectFromHost();
}

void RelayClient::onConnected() {
    qDebug() << "Da ket noi thanh cong toi Relay Server!";
    emit connected();
}

void RelayClient::onDisconnected() {
    emit disconnected();
}

void RelayClient::onReadyRead() {
    const QByteArray data = m_socket->readAll();
    qDebug() << "Nhan duoc" << data.size() << "bytes tu Server!";
    if (!data.isEmpty())
        emit bytesReceived(data);
}

qint64 RelayClient::sendRawPacket(const QByteArray &data)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return -1;
    if (data.isEmpty())
        return -1;
    return m_socket->write(data);
}

qint64 RelayClient::pendingBytes() const
{
    return m_socket->bytesToWrite();
}
