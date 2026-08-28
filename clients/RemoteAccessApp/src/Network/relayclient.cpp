#include "relayclient.h"
#include <QDebug>
#include "protocol/protocolheader.h"
#include "protocol/protocolserializer.h"
#include "protocol/protocolconstants.h"

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
    qDebug() << "Đã kết nối thành công tới Relay Server!";
    sendRegisterHostRequest(9999);
}

void RelayClient::onDisconnected() {
}

void RelayClient::onReadyRead() {
    QByteArray data = m_socket->readAll();
    qDebug() << "Nhận được" << data.size() << "bytes từ Server!";
}

void RelayClient::sendRegisterHostRequest(uint64_t mySessionId)
{
    Protocol::ProtocolHeader header(Protocol::MessageType::MOUSE_MOVE);
    
    header.sessionId = mySessionId;
    header.sequenceNumber = 1;

    QByteArray dataToSend = Protocol::ProtocolSerializer::serializeHeader(header);
    
    if (!dataToSend.isEmpty())
    {
        m_socket->write(dataToSend);
        m_socket->flush();
    }
    else
        qDebug() << "Lỗi: Không thể mã hoá Header!";
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
