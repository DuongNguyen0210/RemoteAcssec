#include "RelayClient.h"
#include <QDebug>
#include <QSslConfiguration>

RelayClient::RelayClient(QObject *parent) : QObject(parent) {
    const QString mode = qEnvironmentVariable("REMOTE_RELAY_TLS", "off");
    m_tlsRequired = mode == QStringLiteral("required");
    m_caCertificatePath = qEnvironmentVariable("REMOTE_RELAY_CA_CERT");
    m_tlsModeValid = m_tlsRequired || (mode == QStringLiteral("off") && m_caCertificatePath.isEmpty());
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
    if (!m_tlsModeValid) {
        emit transportError(QStringLiteral("Set REMOTE_RELAY_TLS=required for a CA certificate; otherwise use 'off'."),
                            QAbstractSocket::SslHandshakeFailedError);
        return;
    }
    if (!m_tlsRequired) {
        m_socket->connectToHost(host, port);
        return;
    }
    QSslConfiguration configuration = m_socket->sslConfiguration();
    if (!m_caCertificatePath.isEmpty() && !configuration.addCaCertificates(m_caCertificatePath)) {
        emit transportError(QStringLiteral("Cannot load REMOTE_RELAY_CA_CERT."),
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
    Q_UNUSED(socketError);
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
