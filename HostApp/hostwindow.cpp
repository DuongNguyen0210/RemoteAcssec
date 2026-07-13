#include "hostwindow.h"

#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QBuffer>
#include <QDataStream>

HostWindow::HostWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Remote Host");
    resize(400, 200);

    auto *titleLabel = new QLabel("Remote Host", this);
    titleLabel->setStyleSheet("font-size: 22px; font-weight: bold;");

    startButton = new QPushButton("Start Sharing", this);

    statusLabel = new QLabel("Status: Waiting", this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(titleLabel);
    layout->addWidget(startButton);
    layout->addWidget(statusLabel);

    setLayout(layout);

    connect(startButton, &QPushButton::clicked, this, &HostWindow::onStartSharingClicked);

    server = new QTcpServer(this);
    clientSocket = nullptr;

    captureTimer = new QTimer(this);
    captureTimer->setInterval(10);

    connect(server, &QTcpServer::newConnection, this, &HostWindow::onNewConnection);
    connect(captureTimer, &QTimer::timeout, this, &HostWindow::sendScreenFrame);

}

HostWindow::~HostWindow() {}

void HostWindow::onStartSharingClicked()
{
    const quint64 port = 9000;
    if(!server->listen(QHostAddress::Any, port))
    {
        statusLabel->setText("Server start failed");
        return;
    }

    statusLabel->setText("Waiting for viewer on port 9000");
    startButton->setEnabled(false);
}

void HostWindow::onNewConnection()
{
    clientSocket = server->nextPendingConnection();
    statusLabel->setText("Status: Viewer connected");

    connect(clientSocket, &QTcpSocket::disconnected, this, [this](){
        statusLabel->setText("Status: Viewer disconnected");
        captureTimer->stop();
        clientSocket->deleteLater();
        clientSocket = nullptr;
    });

    captureTimer->start();
}

void HostWindow::sendScreenFrame()
{
    if (clientSocket == nullptr)
        return;

    if (clientSocket->state() != QAbstractSocket::ConnectedState)
        return;

    QScreen *screen = QGuiApplication::primaryScreen();

    if (screen == nullptr)
    {
        statusLabel->setText("No screen found");
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
    QByteArray textBytes = myText.toUtf8(); // Nội dung gói tin

    QByteArray header; // Tạo mảng byte để chứa 4 byte kích thước
    QDataStream out(&header, QIODevice::WriteOnly); // Khởi tạo luồng ghi vào header
    out.setVersion(QDataStream::Qt_6_0); // Cài đặt version (rất quan trọng trong Qt)

    out << static_cast<quint32>(textBytes.size()); // Ghi độ dài của text vào header

    clientSocket->write(header);     // 1. Gửi đi 4 bytes (Header)
    clientSocket->write(textBytes);  // 2. Gửi đi 16 bytes chữ (Payload)
    clientSocket->flush();           // 3. Đẩy gói tin vào mạng ngay lập tức

}




