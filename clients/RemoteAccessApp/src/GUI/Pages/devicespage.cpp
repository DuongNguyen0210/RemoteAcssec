#include "devicespage.h"
#include "Layouts/flowlayout.h"
#include "Components/devicecardwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>

DevicesPage::DevicesPage(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void DevicesPage::setupUi()
{
    setObjectName("devicesPage");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QVBoxLayout *headerLayout = new QVBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 8);
    headerLayout->setSpacing(4);
    QLabel *lblTitle = new QLabel("Managed Devices", this);
    lblTitle->setObjectName("pageTitle");
    QLabel *lblSubtitle = new QLabel("Monitor and connect to managed remote desktop devices.", this);
    lblSubtitle->setObjectName("pageSubtitle");
    headerLayout->addWidget(lblTitle);
    headerLayout->addWidget(lblSubtitle);
    mainLayout->addLayout(headerLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("deviceScrollArea");
    scrollArea->setWidgetResizable(true);

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setObjectName("deviceScrollContent");
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);

    FlowLayout *flowLayout = new FlowLayout(scrollContent, 0, 16, 16);

    for (int i = 0; i < 6; ++i)
    {
        QString deviceName = QString("WKSTN-DEV-%1").arg(i + 1);
        QString ipAddress = QString("192.168.1.%1").arg(100 + i);
        QString Os = QString("Window 11");
        QString Status = QString("Online");
        QString Uptime = QString("Now");

        DeviceCardWidget *card = new DeviceCardWidget(deviceName, Os, ipAddress, Status, Uptime,  this);
        flowLayout->addWidget(card);
    }

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);
}

