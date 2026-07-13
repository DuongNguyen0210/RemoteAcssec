#include "ViewerWindow.h"

#include "ViewerClient.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QTextEdit>
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

    client = new ViewerClient(this);

    connect(client, &ViewerClient::connected, this, &ViewerWindow::onSocketConnected);
    connect(client, &ViewerClient::disconnected, this, &ViewerWindow::onSocketDisconnected);
    connect(client, &ViewerClient::frameReceived, this, &ViewerWindow::onFrameReceived);

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

    client->connectToHost(ip, port);
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

void ViewerWindow::onFrameReceived(const QByteArray &imageBuffer)
{
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
}
