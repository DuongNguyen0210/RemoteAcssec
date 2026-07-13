#include "HostWindow.h"

#include "HostServer.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

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

    server = new HostServer(this);

    connect(startButton, &QPushButton::clicked, this, &HostWindow::onStartSharingClicked);
    connect(server, &HostServer::statusChanged, statusLabel, &QLabel::setText);
}

HostWindow::~HostWindow() {}

void HostWindow::onStartSharingClicked()
{
    const quint64 port = 9000;
    if(!server->start(port))
    {
        statusLabel->setText("Server start failed");
        return;
    }

    statusLabel->setText("Waiting for viewer on port 9000");
    startButton->setEnabled(false);
}
