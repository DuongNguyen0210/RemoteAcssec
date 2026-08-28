#include "devicespage.h"

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include "../Components/devicecardwidget.h"
#include "../Layouts/flowlayout.h"

DevicesPage::DevicesPage(QWidget *parent)
    : QWidget{parent}
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

    QLabel *lblTitle = new QLabel("Devices", this);
    lblTitle->setProperty("role", "pageTitle");

    QLabel *lblSubtitle = new QLabel("Monitor and manage all registered remote devices.", this);
    lblSubtitle->setProperty("role", "pageSubtitle");

    mainLayout->addWidget(lblTitle);
    mainLayout->addWidget(lblSubtitle);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setProperty("role", "scrollArea");
    scrollArea->setWidgetResizable(true);

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setProperty("role", "scrollContent");
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);

    FlowLayout *flowLayout = new FlowLayout(scrollContent, 16, 16, 16);
    flowLayout->addWidget(new DeviceCardWidget("SRV-APOLLO-01", "Windows Server 2022", "192.168.1.105", "Online", "14d 6h 23m", scrollContent));
    flowLayout->addWidget(new DeviceCardWidget("MBP-SARAH-DESIGN", "macOS Sonoma", "10.0.0.42", "Online", "2d 11h 05m", scrollContent));
    flowLayout->addWidget(new DeviceCardWidget("WKSTN-DEV-04", "Ubuntu 22.04 LTS", "192.168.1.104", "Offline", "—", scrollContent));
    flowLayout->addWidget(new DeviceCardWidget("LAPTOP-MIKE-SALES", "Windows 11 Pro", "10.0.0.55", "Warning", "0d 4h 12m", scrollContent));

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
}
