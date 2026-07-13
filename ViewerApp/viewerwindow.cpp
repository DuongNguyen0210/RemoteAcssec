#include "viewerwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QString>
#include <QTextEdit>

#include <QTcpSocket>
#include <QDataStream>
#include <QPixmap>
#include <QVBoxLayout>


ViewerWindow::ViewerWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("ViewApp");
    resize(400, 200);

    auto *partnerIP = new QFormLayout();
    partnerInput = new QLineEdit(this);
    partnerPort = new QLineEdit(this);
    partnerIP->addRow("PartnerIP", partnerInput);
    partnerIP->addRow("Post", partnerPort);

    auto *connet_status = new QHBoxLayout();
    statusLabel = new QLabel(this);
    bttConnect = new QPushButton(this);
    statusLabel->setText("Disconnect");
    bttConnect->setText("Connect");
    connet_status->addWidget(bttConnect);
    connet_status->addWidget(statusLabel);

    screenFrame = new QFrame(this);
    screenFrame->setMinimumHeight(300);
    screenFrame->setFrameShape(QFrame::Box);

    screenLabel = new QLabel(screenFrame);
    screenLabel->setAlignment(Qt::AlignCenter);
    screenLabel->setStyleSheet("background-color: black; color: white;");
    screenLabel->setText("Remote screen will appear here");

    auto *screenLayout = new QVBoxLayout(screenFrame);
    screenLayout->addWidget(screenLabel);
    screenFrame->setLayout(screenLayout);

    logText = new QTextEdit(this);
    logText->setReadOnly(true);

    auto *layout = new QVBoxLayout(this);

    layout->addLayout(partnerIP);
    layout->addLayout(connet_status);
    layout->addWidget(screenFrame);
    layout->addWidget(logText);

    setLayout(layout);

    socket = new QTcpSocket(this);
    expectedImageSize = 0;

    connect(socket, &QTcpSocket::connected, this, &ViewerWindow::onSocketConnected);
    connect(socket, &QTcpSocket::disconnected, this, &ViewerWindow::onSocketDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &ViewerWindow::onReadyRead);

    connect(bttConnect, &QPushButton::clicked, this, &ViewerWindow::onConnectClicked);
}

ViewerWindow::~ViewerWindow() {}

void ViewerWindow::onConnectClicked()
{
    QString ip = partnerInput->text();
    QString portText = partnerPort->text();

    if(ip.isEmpty())
    {
        logText->append("Missing Partner IP");
        return;
    }

    if(portText.isEmpty())
    {
        logText->append("Missing Port");
        return;
    }

    quint16 port = portText.toUShort();

    QString message = "Trying to connect to " + ip + ":" + port;
    statusLabel->setText(message);
    logText->append(message);

    socket->connectToHost(ip, port);
}

void ViewerWindow::onSocketConnected()
{
    statusLabel->setText("Status: Connected");
    logText->append("Connected to Host");
}

void ViewerWindow::onSocketDisconnected()
{
    statusLabel->setText("Status: Disconnected");
    logText->append("Disconnected from Host");
}

void ViewerWindow::onReadyRead()
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

        QPixmap pixmap;
        bool loaded = pixmap.loadFromData(imageBuffer, "JPG");

        if (loaded)
        {
            QPixmap scaled = pixmap.scaled(
                screenLabel->size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
                );

            screenLabel->setPixmap(scaled);
        }
        else
            logText->append("Failed to load image frame");

        expectedImageSize = 0;
        imageBuffer.clear();
    }
}







