#include "sidebarwidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>

SidebarWidget::SidebarWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void SidebarWidget::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QHBoxLayout *LogoLayout = new QHBoxLayout();
    LogoLayout->addStretch();
    QLabel *logoLabel = new QLabel("<b>RemoteAccess</b><br>Admin User", this);
    logoLabel->setFixedHeight(100);
    LogoLayout->addWidget(logoLabel);
    LogoLayout->addStretch();

    layout->addLayout(LogoLayout);

    QFrame *lineTop = new QFrame(this);
    lineTop->setFrameShape(QFrame::HLine);
    layout->addWidget(lineTop);

    QVBoxLayout *MenuLayout = new QVBoxLayout();
    MenuLayout->setContentsMargins(0, 10, 0, 10);
    MenuLayout->setSpacing(0);
    QPushButton *btnDevices = new QPushButton("Devices", this);
    QPushButton *btnSessions = new QPushButton("Sessions", this);
    QPushButton *btnSettings = new QPushButton("Settings", this);
    QPushButton *btnLogs = new QPushButton("Logs", this);
    MenuLayout->addWidget(btnDevices);
    MenuLayout->addWidget(btnSessions);
    MenuLayout->addWidget(btnSettings);
    MenuLayout->addWidget(btnLogs);

    layout->addLayout(MenuLayout);


    QFrame *lineBottom = new QFrame(this);
    lineBottom->setFrameShape(QFrame::HLine);
    layout->addWidget(lineBottom);

    QVBoxLayout *ConnLayout = new QVBoxLayout();
    ConnLayout->setContentsMargins(0, 10, 0, 0);
    QPushButton *btnNewConn = new QPushButton("Add New Connection", this);
    ConnLayout->addWidget(btnNewConn);

    layout->addLayout(ConnLayout);
    layout->addStretch();

    connect(btnDevices, &QPushButton::clicked, this, [this](){ emit pageChanged(0); });
    connect(btnSessions, &QPushButton::clicked, this, [this](){ emit pageChanged(1); });
    connect(btnSettings, &QPushButton::clicked, this, [this](){ emit pageChanged(2); });
    connect(btnLogs, &QPushButton::clicked, this, [this](){ emit pageChanged(3); });
}
