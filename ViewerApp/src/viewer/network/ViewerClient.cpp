#include "ViewerClient.h"

#include <QDataStream>
#include <QTcpSocket>

ViewerClient::ViewerClient(QObject *parent)
    : QObject(parent)
{
    socket = new QTcpSocket(this);
    expectedImageSize = 0;

    connect(socket, &QTcpSocket::connected, this, &ViewerClient::connected);
    connect(socket, &QTcpSocket::disconnected, this, &ViewerClient::disconnected);
    connect(socket, &QTcpSocket::readyRead, this, &ViewerClient::onReadyRead);
}

void ViewerClient::connectToHost(const QString &ip, quint16 port)
{
    socket->connectToHost(ip, port);
}

void ViewerClient::onReadyRead()
{
    while (socket->bytesAvailable() > 0)
    {
        if (expectedImageSize == 0)
        {
            if (socket->bytesAvailable() < static_cast<int>(sizeof(quint32)))
                return;

            QDataStream in(socket);
            in.setVersion(QDataStream::Qt_6_0);
            in >> expectedImageSize;
        }

        if (socket->bytesAvailable() < expectedImageSize)
            return;

        imageBuffer = socket->read(expectedImageSize);
        emit frameReceived(imageBuffer);

        expectedImageSize = 0;
        imageBuffer.clear();
    }
}
