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
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *lblTitle = new QLabel("<h2>Managed Devices</h2>", this);
    headerLayout->addWidget(lblTitle);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background-color: transparent; }");

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("background-color: transparent;");

    FlowLayout *flowLayout = new FlowLayout(scrollContent, 0, 15, 15);

    QList<DeviceCardWidget*> cardList;

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

