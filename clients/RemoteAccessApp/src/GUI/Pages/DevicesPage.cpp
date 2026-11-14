#include "DevicesPage.h"

#include <QLabel>
#include <QPushButton>
#include <QDateTime>
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
    , m_refreshStatus(nullptr)
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

    QLabel *lblTitle = new QLabel("Thiết bị", this);
    lblTitle->setProperty("role", "pageTitle");

    QLabel *lblSubtitle = new QLabel("Các máy đang hoạt động.", this);
    lblSubtitle->setProperty("role", "pageSubtitle");

    mainLayout->addWidget(lblTitle);
    mainLayout->addWidget(lblSubtitle);
    auto *refresh = new QPushButton("Làm mới", this);
    refresh->setFixedHeight(38);
    refresh->setCursor(Qt::PointingHandCursor);
    refresh->setProperty("role", "primaryActionButton");
    connect(refresh, &QPushButton::clicked, this, &DevicesPage::refreshRequested);
    mainLayout->addWidget(refresh, 0, Qt::AlignLeft);
    m_refreshStatus = new QLabel("Đang chờ cập nhật", this);
    m_refreshStatus->setProperty("role", "metaLabel");
    mainLayout->addWidget(m_refreshStatus);

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
    m_allDevices = devices;
    m_refreshStatus->setText("Cập nhật lúc: " + QDateTime::currentDateTime().toString("HH:mm:ss"));
    renderDevices();
}

void DevicesPage::setSearchQuery(const QString &query)
{
    const QString normalized = query.trimmed();
    if (m_searchQuery == normalized) return;
    m_searchQuery = normalized;
    renderDevices();
}

void DevicesPage::renderDevices()
{
    if (!m_flowLayout) return;

    while (QLayoutItem *item = m_flowLayout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (m_allDevices.isEmpty()) {
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

    int matches = 0;
    for (const DeviceInfo &device : m_allDevices) {
        if (!m_searchQuery.isEmpty()
                && !device.deviceName.contains(m_searchQuery, Qt::CaseInsensitive)
                && !device.username.contains(m_searchQuery, Qt::CaseInsensitive)
                && !device.os.contains(m_searchQuery, Qt::CaseInsensitive)
                && !device.ipAddress.contains(m_searchQuery, Qt::CaseInsensitive)) continue;
        DeviceCardWidget *card = new DeviceCardWidget(device, m_scrollContent);
        connect(card, &DeviceCardWidget::connectRequested,
                this, &DevicesPage::connectRequested);
        m_flowLayout->addWidget(card);
        ++matches;
    }
    if (matches == 0) {
        auto *emptyState = new EmptyStateWidget("Không tìm thấy thiết bị",
            "Thử từ khóa khác hoặc xóa ô tìm kiếm.", "Xóa tìm kiếm", m_scrollContent);
        connect(emptyState, &EmptyStateWidget::actionClicked,
                this, &DevicesPage::clearSearchRequested);
        m_flowLayout->addWidget(emptyState);
    }

    qDebug() << "[DevicesPage] Rendered" << matches << "of" << m_allDevices.size() << "devices.";
}

void DevicesPage::showError(const QString &message)
{
    m_refreshStatus->setText("Chưa cập nhật được. Dữ liệu có thể đã cũ.");
    ConfirmDialog::showWarning(this, QStringLiteral("Lỗi"), message);
}

void DevicesPage::showSuccess(const QString &message)
{
    ConfirmDialog::showInfo(this, QStringLiteral("Thành công"), message);
}
