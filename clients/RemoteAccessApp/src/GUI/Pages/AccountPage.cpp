#include "AccountPage.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QMessageBox>
#include "GUI/Components/EmptyStateWidget.h"

namespace {

QLabel *label(const QString &text, const QString &objectName, QWidget *parent)
{
    QLabel *lbl = new QLabel(text, parent);
    lbl->setProperty("role", objectName);
    return lbl;
}

QPushButton *actionButton(const QString &text, const QString &objectName, QWidget *parent)
{
    QPushButton *button = new QPushButton(text, parent);
    button->setProperty("role", objectName);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(36);
    return button;
}

QFrame *accountCard(const QString &username, const QString &password, const QString &role,
                    const QString &status, const QString &statusState, QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setProperty("role", "sessionCard");
    card->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout();
    header->setSpacing(12);

    QLabel *avatar = label(QString(username.isEmpty() ? '?' : username[0]).toUpper(), "cardIcon", card);
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    header->addWidget(avatar);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    titleLayout->addWidget(label(username, "cardTitle", card));

    QHBoxLayout *passwordLayout = new QHBoxLayout();
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->setSpacing(8);
    QLabel *passwordLabel = label("••••••••", "cardSubtitle", card);
    passwordLayout->addWidget(passwordLabel);

    QPushButton *togglePwdBtn = new QPushButton("Show", card);
    togglePwdBtn->setCursor(Qt::PointingHandCursor);
    togglePwdBtn->setProperty("role", "linkButton");
    togglePwdBtn->setFixedWidth(40);
    passwordLayout->addWidget(togglePwdBtn);
    passwordLayout->addStretch();

    QObject::connect(togglePwdBtn, &QPushButton::clicked, [passwordLabel, togglePwdBtn, password]() {
        if (passwordLabel->text() == "••••••••") {
            passwordLabel->setText(password);
            togglePwdBtn->setText("Hide");
        } else {
            passwordLabel->setText("••••••••");
            togglePwdBtn->setText("Show");
        }
    });

    titleLayout->addLayout(passwordLayout);
    header->addLayout(titleLayout, 1);

    QLabel *chip = label(status, "stateChip", card);
    chip->setProperty("state", statusState);
    chip->setAlignment(Qt::AlignCenter);
    header->addWidget(chip);

    layout->addLayout(header);

    QGridLayout *details = new QGridLayout();
    details->setHorizontalSpacing(24);
    details->setVerticalSpacing(4);
    details->addWidget(label("ROLE",   "metaLabel", card), 0, 0);
    details->addWidget(label("STATUS", "metaLabel", card), 0, 1);
    details->addWidget(label(role,     "strongText", card), 1, 0);
    details->addWidget(label(status,   "strongText", card), 1, 1);
    layout->addLayout(details);

    QFrame *divider = new QFrame(card);
    divider->setProperty("role", "thinDivider");
    divider->setFrameShape(QFrame::HLine);
    layout->addWidget(divider);

    QHBoxLayout *actions = new QHBoxLayout();
    actions->setSpacing(10);
    actions->addWidget(actionButton("Edit",   "secondaryActionButton", card));
    actions->addWidget(actionButton("Delete", "dangerActionButton",    card));
    actions->addStretch();
    layout->addLayout(actions);

    return card;
}

}

AccountPage::AccountPage(QWidget *parent)
    : QWidget{parent}
    , m_totalAccountsVal(nullptr)
    , m_activeAccountsVal(nullptr)
    , m_inactiveAccountsVal(nullptr)
    , m_searchInput(nullptr)
    , m_listLayout(nullptr)
    , m_scrollContent(nullptr)
{
    setupUi();
}

void AccountPage::loadData()
{
    emit loadRequested();
}

void AccountPage::showLoading()
{
    if (!m_listLayout || !m_scrollContent) return;

    QLayoutItem *child;
    while ((child = m_listLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    QLabel *lbl = new QLabel("Đang tải dữ liệu...", m_scrollContent);
    lbl->setProperty("role", "metaLabel");
    m_listLayout->addWidget(lbl);
    m_listLayout->addStretch();
}

void AccountPage::updateAccountList(const QList<DeviceInfo> &accounts)
{
    m_allAccounts = accounts;
    updateMetrics();
    renderAccounts(m_searchInput ? m_searchInput->text() : QString());
}

void AccountPage::updateMetrics()
{
    int total = m_allAccounts.size();
    int active = 0;
    for (const DeviceInfo &acc : m_allAccounts) {
        if (acc.isOnline) {
            active++;
        }
    }
    int inactive = total - active;

    if (m_totalAccountsVal) {
        m_totalAccountsVal->setText(QString::number(total));
    }
    if (m_activeAccountsVal) {
        m_activeAccountsVal->setText(QString::number(active));
    }
    if (m_inactiveAccountsVal) {
        m_inactiveAccountsVal->setText(QString::number(inactive));
    }
}

void AccountPage::onSearchTextChanged(const QString &text)
{
    renderAccounts(text);
}

void AccountPage::renderAccounts(const QString &filterText)
{
    if (!m_listLayout || !m_scrollContent) return;

    QLayoutItem *child;
    while ((child = m_listLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    if (m_allAccounts.isEmpty()) {
        EmptyStateWidget *emptyState = new EmptyStateWidget(
            "Chưa có tài khoản con nào",
            "Bạn chưa tạo tài khoản máy con nào. Hãy nhấn nút Thêm tài khoản để bắt đầu.",
            "Thêm tài khoản",
            m_scrollContent
        );
        connect(emptyState, &EmptyStateWidget::actionClicked,
                this, &AccountPage::requestAddAccount);
        m_listLayout->addWidget(emptyState);
        return;
    }

    QString query = filterText.trimmed();
    QList<DeviceInfo> filtered;
    for (const DeviceInfo &acc : m_allAccounts) {
        QString username = acc.childUsername.isEmpty() ? acc.username : acc.childUsername;
        if (query.isEmpty() || username.contains(query, Qt::CaseInsensitive)) {
            filtered.append(acc);
        }
    }

    if (filtered.isEmpty()) {
        EmptyStateWidget *emptyState = new EmptyStateWidget(
            "Không tìm thấy tài khoản",
            QString("Không có tài khoản nào khớp với \"%1\".").arg(query),
            "Xóa bộ lọc",
            m_scrollContent
        );
        connect(emptyState, &EmptyStateWidget::actionClicked, this, [this]() {
            if (m_searchInput) m_searchInput->clear();
        });
        m_listLayout->addWidget(emptyState);
        return;
    }

    for (const DeviceInfo &acc : filtered) {
        QString username = acc.childUsername.isEmpty() ? acc.username : acc.childUsername;
        QString password = acc.password.isEmpty() ? "N/A" : acc.password;
        QString statusText = acc.isOnline ? "Active" : "Offline";
        QString statusRole = acc.isOnline ? "active" : "offline";

        m_listLayout->addWidget(accountCard(username, password, "Child", statusText, statusRole, m_scrollContent));
    }

    m_listLayout->addStretch();
}

void AccountPage::showError(const QString &message)
{
    QMessageBox::warning(this, "Lỗi", message);
}

void AccountPage::setupUi()
{
    setObjectName("accountPage");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QVBoxLayout *header = new QVBoxLayout();
    header->setContentsMargins(0, 0, 0, 8);
    header->setSpacing(4);
    header->addWidget(label("Sub-Accounts",   "pageTitle",    this));
    header->addWidget(label("Manage sub-accounts linked to your administrator account. You can add, edit, or remove them below.",
                            "pageSubtitle", this));
    mainLayout->addLayout(header);

    QGridLayout *metrics = new QGridLayout();
    metrics->setHorizontalSpacing(16);
    metrics->setVerticalSpacing(16);

    auto createMetricCard = [this](const QString &initialValue, const QString &title,
                                   const QString &detail, QLabel *&valLabelTarget) -> QFrame *
    {
        QFrame *card = new QFrame(this);
        card->setProperty("role", "metricCard");
        card->setAttribute(Qt::WA_StyledBackground, true);
        QVBoxLayout *l = new QVBoxLayout(card);
        l->setContentsMargins(16, 14, 16, 14);
        l->setSpacing(4);

        valLabelTarget = new QLabel(initialValue, card);
        valLabelTarget->setProperty("role", "metricValue");

        QLabel *titleLbl = new QLabel(title, card);
        titleLbl->setProperty("role", "metricTitle");

        QLabel *detailLbl = new QLabel(detail, card);
        detailLbl->setProperty("role", "metricDetail");

        l->addWidget(valLabelTarget);
        l->addWidget(titleLbl);
        l->addWidget(detailLbl);
        return card;
    };

    metrics->addWidget(createMetricCard("0", "Total sub-accounts", "Under your administrator account", m_totalAccountsVal), 0, 0);
    metrics->addWidget(createMetricCard("0", "Active accounts", "Currently enabled", m_activeAccountsVal), 0, 1);
    metrics->addWidget(createMetricCard("0", "Inactive accounts", "Disabled or suspended", m_inactiveAccountsVal), 0, 2);
    mainLayout->addLayout(metrics);

    QFrame *toolbar = new QFrame(this);
    toolbar->setProperty("role", "toolbarCard");
    toolbar->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 12, 12, 12);
    toolbarLayout->setSpacing(8);

    m_searchInput = new QLineEdit(toolbar);
    m_searchInput->setProperty("role", "panelSearchInput");
    m_searchInput->setPlaceholderText("Search accounts...");
    m_searchInput->setClearButtonEnabled(true);
    m_searchInput->setFixedHeight(38);
    toolbarLayout->addWidget(m_searchInput, 1);

    connect(m_searchInput, &QLineEdit::textChanged,
            this, &AccountPage::onSearchTextChanged);

    QPushButton *addButton = new QPushButton("Add Account", toolbar);
    addButton->setProperty("role", "primaryActionButton");
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setFixedHeight(38);
    toolbarLayout->addWidget(addButton);

    connect(addButton, &QPushButton::clicked, this, [this](){
        emit requestAddAccount();
    });

    mainLayout->addWidget(toolbar);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setProperty("role", "scrollArea");
    scrollArea->setWidgetResizable(true);

    m_scrollContent = new QWidget(scrollArea);
    m_scrollContent->setProperty("role", "scrollContent");
    m_scrollContent->setAttribute(Qt::WA_StyledBackground, true);

    m_listLayout = new QVBoxLayout(m_scrollContent);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(14);

    m_listLayout->addWidget(label("Đang tải dữ liệu...", "metaLabel", m_scrollContent));
    m_listLayout->addStretch();

    scrollArea->setWidget(m_scrollContent);
    mainLayout->addWidget(scrollArea, 1);
}
