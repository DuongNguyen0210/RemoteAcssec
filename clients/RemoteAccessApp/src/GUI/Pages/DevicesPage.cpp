#include "DevicesPage.h"

#include <QLabel>
#include <QDebug>
#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>
#include "GUI/Dialogs/ConfirmDialog.h"
#include "GUI/Components/DeviceCardWidget.h"
#include "GUI/Components/EmptyStateWidget.h"
#include "GUI/Layouts/FlowLayout.h"

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

    if (devices.isEmpty()) {
        EmptyStateWidget *emptyState = new EmptyStateWidget(
            "Chưa có thiết bị nào",
            "Hiện tại chưa có máy con nào kết nối đến hệ thống.",
            "Làm mới",
            m_scrollContent
        );
        connect(emptyState, &EmptyStateWidget::actionClicked,
                this, &DevicesPage::refreshRequested);
        m_flowLayout->addWidget(emptyState);
        return;
    }

    for (const DeviceInfo &device : devices) {
        DeviceCardWidget *card = new DeviceCardWidget(device, m_scrollContent);
        connect(card, &DeviceCardWidget::connectRequested,
                this, &DevicesPage::connectRequested);
        connect(card, &DeviceCardWidget::removeRequested,
                this, &DevicesPage::removeDeviceRequested);
        m_flowLayout->addWidget(card);
    }

    qDebug() << "[DevicesPage] Rendered" << devices.size() << "devices.";
}

void DevicesPage::showError(const QString &message)
{
    ConfirmDialog::showWarning(this, QStringLiteral("Lỗi"), message);
}

void DevicesPage::showSuccess(const QString &message)
{
    ConfirmDialog::showInfo(this, QStringLiteral("Thành công"), message);
}
