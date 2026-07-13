#include "HostServer.h"

#include <QBuffer>
#include <QDataStream>
#include <QGuiApplication>
#include <QHostAddress>
#include <QPixmap>
#include <QScreen>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

HostServer::HostServer(QObject *parent)
    : QObject(parent)
{
    server = new QTcpServer(this);
    clientSocket = nullptr;

    captureTimer = new QTimer(this);
    captureTimer->setInterval(10);

    connect(server, &QTcpServer::newConnection, this, &HostServer::onNewConnection);
    connect(captureTimer, &QTimer::timeout, this, &HostServer::sendScreenFrame);
}

bool HostServer::start(quint16 port)
{
    return server->listen(QHostAddress::Any, port);
}

void HostServer::onNewConnection()
{
    clientSocket = server->nextPendingConnection();
    emit statusChanged("Status: Viewer connected");

    connect(clientSocket, &QTcpSocket::disconnected, this, [this](){
        emit statusChanged("Status: Viewer disconnected");
        captureTimer->stop();
        clientSocket->deleteLater();
        clientSocket = nullptr;
    });

    captureTimer->start();
}

void HostServer::sendScreenFrame()
{
    if (clientSocket == nullptr)
        return;

    if (clientSocket->state() != QAbstractSocket::ConnectedState)
        return;

    QScreen *screen = QGuiApplication::primaryScreen();

    if (screen == nullptr)
    {
        emit statusChanged("No screen found");
        return;
    }

    // QPixmap screenshot = screen->grabWindow(0);

    // QByteArray imageBytes;
    // QBuffer buffer(&imageBytes);
    // buffer.open(QIODevice::WriteOnly);

    // screenshot.save(&buffer, "JPG", 100);

    // QByteArray packet;
    // QDataStream out(&packet, QIODevice::WriteOnly);
    // out.setVersion(QDataStream::Qt_6_0);

    // out << static_cast<quint32>(imageBytes.size());

    // clientSocket->write(packet);
    // clientSocket->write(imageBytes);
    // clientSocket->flush();

    QString myText = "Hello Wireshark!";
    QByteArray textBytes = myText.toUtf8();

    QByteArray header;
    QDataStream out(&header, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);

    out << static_cast<quint32>(textBytes.size());

    clientSocket->write(header);
    clientSocket->write(textBytes);
    clientSocket->flush();
}
