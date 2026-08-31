#include "devicespage.h"

#include <QLabel>
#include <QDebug>
#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>
#include "../Components/devicecardwidget.h"
#include "../Layouts/flowlayout.h"
#include "../../Network/childdiscoveryservice.h"

DevicesPage::DevicesPage(QWidget *parent)
    : QWidget{parent}
    , m_childDiscoveryService(new ChildDiscoveryService(this))
    , m_flowLayout(nullptr)
    , m_scrollContent(nullptr)
{
    setupUi();
    connect(m_childDiscoveryService, &ChildDiscoveryService::childrenLoaded,
            this, &DevicesPage::handleChildrenLoaded);
    connect(m_childDiscoveryService, &ChildDiscoveryService::loadFailed,
            this, &DevicesPage::handleLoadFailed);
    m_childDiscoveryService->loadChildren();
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

void DevicesPage::handleChildrenLoaded(const QStringList &childUsernames)
{
    while (QLayoutItem *item = m_flowLayout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (const QString &childUsername : childUsernames) {
        DeviceCardWidget *card = new DeviceCardWidget(
                childUsername,
                childUsername,
                QStringLiteral("Thông tin thiết bị chưa được cung cấp"),
                QStringLiteral("Chưa được cung cấp"),
                QStringLiteral("Không xác định"),
                QStringLiteral("Chưa được cung cấp"),
                m_scrollContent);
        connect(card, &DeviceCardWidget::connectRequested,
                this, &DevicesPage::connectRequested);
        m_flowLayout->addWidget(card);
    }

    qDebug() << "[DevicesPage] Da tai" << childUsernames.size()
             << "tai khoan CHILD cua ADMIN.";
}

void DevicesPage::handleLoadFailed(const QString &message)
{
    qWarning() << "[DevicesPage]" << message;
}
