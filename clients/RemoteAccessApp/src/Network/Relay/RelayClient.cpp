#include "RelayClient.h"
#include "../TransportPolicy.h"
#include <QSslConfiguration>

RelayClient::RelayClient(QObject *parent) : QObject(parent) {
    m_socket = new QSslSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &RelayClient::onConnected);
    connect(m_socket, &QSslSocket::encrypted, this, [this] { emit connected(); });
    connect(m_socket, &QTcpSocket::disconnected, this, &RelayClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &RelayClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &RelayClient::onErrorOccurred);
}

RelayClient::~RelayClient() {
}

void RelayClient::ConnectToServer(const QString& host, quint16 port)
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        emit transportError(QStringLiteral("Kết nối relay đang được thiết lập hoặc đã mở."),
                            QAbstractSocket::OperationError);
        return;
    }
    const QString caPath = qEnvironmentVariable("REMOTE_RELAY_CA_CERT");
    const QString defaultMode = TransportPolicy::isLoopback(host) && caPath.isEmpty()
        ? QStringLiteral("off") : QStringLiteral("required");
    const QString mode = qEnvironmentVariable("REMOTE_RELAY_TLS", defaultMode);
    if (mode != QStringLiteral("required") && (mode != QStringLiteral("off") || !caPath.isEmpty())) {
        emit transportError(QStringLiteral("Kiểm tra REMOTE_RELAY_TLS. Dùng required khi đã cấu hình chứng chỉ CA."),
                            QAbstractSocket::SslHandshakeFailedError);
        return;
    }
    m_tlsRequired = mode == QStringLiteral("required");
    if (!m_tlsRequired) {
        m_socket->connectToHost(host, port);
        return;
    }
    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    if (!caPath.isEmpty() && !configuration.addCaCertificates(caPath)) {
        emit transportError(QStringLiteral("Không đọc được chứng chỉ trong REMOTE_RELAY_CA_CERT."),
                            QAbstractSocket::SslHandshakeFailedError);
        return;
    }
    m_socket->setSslConfiguration(configuration);
    m_socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_socket->connectToHostEncrypted(host, port);
}

void RelayClient::DisconnectFromServer()
{
    m_socket->abort();
}

void RelayClient::onConnected() {
    if (!m_tlsRequired) emit connected();
}

void RelayClient::onDisconnected() {
    emit disconnected();
}

void RelayClient::onReadyRead() {
    const QByteArray data = m_socket->readAll();
    if (!data.isEmpty())
        emit bytesReceived(data);
}

void RelayClient::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    emit transportError(m_socket->errorString(), socketError);
}

qint64 RelayClient::sendRawPacket(const QByteArray &data)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState || (m_tlsRequired && !m_socket->isEncrypted()))
        return -1;
    if (data.isEmpty())
        return -1;
    return m_socket->write(data);
}

qint64 RelayClient::pendingBytes() const
{
    return m_socket->bytesToWrite();
}
