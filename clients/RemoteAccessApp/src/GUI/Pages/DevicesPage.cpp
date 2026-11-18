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
    m_refreshButton = new QPushButton("Làm mới", this);
    m_refreshButton->setFixedHeight(38);
    m_refreshButton->setCursor(Qt::PointingHandCursor);
    m_refreshButton->setProperty("role", "primaryActionButton");
    connect(m_refreshButton, &QPushButton::clicked, this, &DevicesPage::refreshRequested);
    mainLayout->addWidget(m_refreshButton, 0, Qt::AlignLeft);
    m_refreshStatus = new QLabel("Đang chờ cập nhật", this);
    m_refreshStatus->setObjectName("deviceRefreshStatus");
    m_refreshStatus->setProperty("role", "metaLabel");
    mainLayout->addWidget(m_refreshStatus);
    m_sessionStatus = new QLabel(this);
    m_sessionStatus->setObjectName("deviceSessionStatus");
    m_sessionStatus->setProperty("role", "pageSubtitle");
    m_sessionStatus->setTextFormat(Qt::PlainText);
    m_sessionStatus->setWordWrap(true);
    m_sessionStatus->hide();
    mainLayout->addWidget(m_sessionStatus);

    m_liveStatus = new QLabel(QStringLiteral("Đang chờ cập nhật trực tiếp…"), this);
    m_liveStatus->setObjectName("deviceLiveStatus");
    m_liveStatus->setProperty("role", "metaLabel");
    m_liveStatus->setWordWrap(true);
    mainLayout->addWidget(m_liveStatus);

    m_connectionStatus = new QLabel(this);
    m_connectionStatus->setObjectName("connectionStatus");
    m_connectionStatus->setProperty("role", "pageSubtitle");
    m_connectionStatus->setTextFormat(Qt::PlainText);
    m_connectionStatus->setWordWrap(true);
    m_connectionStatus->hide();
    mainLayout->addWidget(m_connectionStatus);

    QScrollArea *scrollArea = new QScrollArea(this);
    m_scrollArea = scrollArea;
    scrollArea->setProperty("role", "scrollArea");
    scrollArea->setWidgetResizable(true);

    m_scrollContent = new QWidget(scrollArea);
    m_scrollContent->setProperty("role", "scrollContent");
    m_scrollContent->setAttribute(Qt::WA_StyledBackground, true);

    m_flowLayout = new FlowLayout(m_scrollContent, 16, 16, 16);

    scrollArea->setWidget(m_scrollContent);
    mainLayout->addWidget(scrollArea, 1);
    m_emptyState = new EmptyStateWidget("Chưa có thiết bị nào",
        "Hiện tại chưa có máy con nào kết nối đến hệ thống.", "Làm mới", this);
    connect(m_emptyState, &EmptyStateWidget::actionClicked, this, [this] {
        if (m_allDevices.isEmpty()) emit refreshRequested();
        else emit clearSearchRequested();
    });
    mainLayout->addWidget(m_emptyState, 1);
    m_emptyState->hide();
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
        if (auto *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    m_refreshButton->setVisible(!m_allDevices.isEmpty());
    if (m_allDevices.isEmpty()) {
        m_emptyState->setTitle("Chưa có thiết bị nào");
        m_emptyState->setDescription("Hiện tại chưa có máy con nào kết nối đến hệ thống.");
        m_emptyState->setActionText("Làm mới");
        m_scrollArea->hide();
        m_emptyState->show();
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
        card->setPendingConnection(m_pendingSessionId);
        connect(card, &DeviceCardWidget::connectRequested, this, [this, name = device.deviceName](const QString &id) {
            if (!m_pendingSessionId.isEmpty() || id.isEmpty())
                return;
            m_pendingSessionId = id;
            for (auto *deviceCard : m_scrollContent->findChildren<DeviceCardWidget *>())
                deviceCard->setPendingConnection(id);
            m_connectionStatus->setText(QStringLiteral("Đang kết nối tới %1…").arg(name));
            m_connectionStatus->show();
            emit connectRequested(id);
        });
        m_flowLayout->addWidget(card);
        ++matches;
    }
    if (matches == 0) {
        m_emptyState->setTitle("Không tìm thấy thiết bị");
        m_emptyState->setDescription("Thử từ khóa khác hoặc xóa ô tìm kiếm.");
        m_emptyState->setActionText("Xóa tìm kiếm");
    }
    m_emptyState->setVisible(matches == 0);
    m_scrollArea->setVisible(matches != 0);

    qDebug() << "[DevicesPage] Rendered" << matches << "of" << m_allDevices.size() << "devices.";
}

void DevicesPage::setLiveUpdatesAvailable(bool available)
{
    m_liveStatus->setText(available
        ? QStringLiteral("Đang cập nhật trực tiếp.")
        : QStringLiteral("Mất cập nhật trực tiếp. Bạn có thể bấm Làm mới để kiểm tra danh sách."));
}

void DevicesPage::finishConnectionRequest(const QString &error)
{
    m_pendingSessionId.clear();
    for (auto *card : m_scrollContent->findChildren<DeviceCardWidget *>())
        card->setPendingConnection({});
    m_connectionStatus->setText(error);
    m_connectionStatus->setVisible(!error.isEmpty());
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

void DevicesPage::showSessionStatus(const QString &message)
{
    m_sessionStatus->setText(message);
    m_sessionStatus->setVisible(!message.isEmpty());
}
