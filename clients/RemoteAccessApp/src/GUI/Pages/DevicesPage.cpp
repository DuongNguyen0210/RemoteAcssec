#include "DevicesPage.h"

#include <QLabel>
#include <QDebug>
#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>
#include "../Components/DevicecardWidget.h"
#include "../Layouts/Flowlayout.h"
#include "../../Network/AccountService.h"
#include <QJsonObject>

DevicesPage::DevicesPage(QWidget *parent)
    : QWidget{parent}
    , m_accountService(new AccountService(this))
    , m_flowLayout(nullptr)
    , m_scrollContent(nullptr)
{
    setupUi();
    connect(m_accountService, &AccountService::fetchListChildrenResult,
            this, &DevicesPage::handleChildrenLoaded);
    loadDevices();
}

void DevicesPage::loadDevices()
{
    m_accountService->fetchListChildren();
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

void DevicesPage::handleChildrenLoaded(bool success, const QJsonArray &children, const QString &message)
{
    if (!m_flowLayout) return;

    while (QLayoutItem *item = m_flowLayout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (!success) {
        qWarning() << "[DevicesPage] Failed to load devices:" << message;
        return;
    }

    int count = 0;
    for (const QJsonValue &val : children) {
        if (!val.isObject()) continue;

        QJsonObject childObj = val.toObject();
        QString childUsername = childObj.value(QStringLiteral("childUsername")).toString();
        if (childUsername.isEmpty()) {
            childUsername = childObj.value(QStringLiteral("username")).toString();
        }
        if (childUsername.isEmpty()) continue;

        bool isOnline = childObj.value(QStringLiteral("online")).toBool(false);
        QString status = isOnline ? QStringLiteral("Active") : QStringLiteral("Offline");

        DeviceCardWidget *card = new DeviceCardWidget(
                childUsername,
                childUsername,
                QStringLiteral("Thông tin thiết bị"),
                QStringLiteral("Local Network"),
                status,
                QStringLiteral("N/A"),
                m_scrollContent);
        connect(card, &DeviceCardWidget::connectRequested,
                this, &DevicesPage::connectRequested);
        m_flowLayout->addWidget(card);
        count++;
    }

    qDebug() << "[DevicesPage] Đã tải" << count << "thiết bị con qua AccountService.";
}
