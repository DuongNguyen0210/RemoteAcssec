#include "DevicesPage.h"

#include <QLabel>
#include <QDebug>
#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>
#include "../Components/DevicecardWidget.h"
#include "../Layouts/Flowlayout.h"

DevicesPage::DevicesPage(QWidget *parent)
    : QWidget{parent}
    , m_flowLayout(nullptr)
    , m_scrollContent(nullptr)
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

    m_scrollContent = new QWidget(scrollArea);
    m_scrollContent->setProperty("role", "scrollContent");
    m_scrollContent->setAttribute(Qt::WA_StyledBackground, true);

    m_flowLayout = new FlowLayout(m_scrollContent, 16, 16, 16);

    scrollArea->setWidget(m_scrollContent);
    mainLayout->addWidget(scrollArea, 1);
}

void DevicesPage::updateDeviceList(const QList<DeviceInfo> &devices)
{
    if (!m_flowLayout) return;

    while (QLayoutItem *item = m_flowLayout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (const DeviceInfo &device : devices) {
        QString status = device.isOnline ? QStringLiteral("Active") : QStringLiteral("Offline");

        DeviceCardWidget *card = new DeviceCardWidget(
                device.childUsername,
                device.childUsername,
                device.os,
                device.ipAddress,
                status,
                QStringLiteral("N/A"),
                m_scrollContent);
        connect(card, &DeviceCardWidget::connectRequested,
                this, &DevicesPage::connectRequested);
        m_flowLayout->addWidget(card);
    }

    qDebug() << "[DevicesPage] Đã render" << devices.size() << "thiết bị từ DeviceInfo Model.";
}
