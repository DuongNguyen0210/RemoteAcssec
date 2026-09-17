#include "AccountPage.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include "GUI/Dialogs/ConfirmDialog.h"
#include "GUI/Components/EmptyStateWidget.h"
#include "GUI/Components/Accounts/AccountCardWidget.h"

AccountPage::AccountPage(QWidget *parent) : QWidget(parent)
{
    setupUi();
    renderAccounts();
}

void AccountPage::loadData() { emit loadRequested(); }
void AccountPage::showLoading()
{
    m_loading = true;
    m_loadError.clear();
    renderAccounts();
}
void AccountPage::updateAccountList(const QList<AccountInfo> &accounts)
{
    m_loading = false;
    m_loadError.clear();
    m_allAccounts = accounts;
    m_totalAccountsVal->setText(QString::number(accounts.size()));
    renderAccounts();
}
void AccountPage::showLoadError(const QString &message)
{
    m_loading = false;
    m_loadError = message;
    renderAccounts();
}
void AccountPage::showError(const QString &message)
{
    ConfirmDialog::showWarning(this, QStringLiteral("Không thực hiện được"), message);
}
void AccountPage::setActionsEnabled(bool enabled)
{
    m_toolbar->setEnabled(enabled);
    m_scrollContent->setEnabled(enabled);
}

void AccountPage::renderAccounts()
{
    while (auto *item = m_listLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }
    const bool hasAccounts = !m_allAccounts.isEmpty();
    m_toolbar->setVisible(hasAccounts && !m_loading);
    m_metrics->setVisible(hasAccounts && !m_loading);
    if (m_loading) {
        auto *state = new EmptyStateWidget("Đang tải tài khoản…", "", "", m_scrollContent);
        state->setIconText("…");
        m_listLayout->addWidget(state, 1);
        return;
    }
    if (!m_loadError.isEmpty()) {
        auto *state = new EmptyStateWidget("Chưa tải được tài khoản", m_loadError,
                                          "Thử lại", m_scrollContent);
        state->setIconText("!");
        connect(state, &EmptyStateWidget::actionClicked, this, &AccountPage::loadRequested);
        m_listLayout->addWidget(state, hasAccounts ? 0 : 1);
        if (!hasAccounts) return;
    }
    if (!hasAccounts) {
        auto *state = new EmptyStateWidget("Chưa có tài khoản", "Thêm tài khoản để kết nối máy con.",
                                          "Thêm tài khoản", m_scrollContent);
        connect(state, &EmptyStateWidget::actionClicked, this, &AccountPage::requestAddAccount);
        m_listLayout->addWidget(state, 1);
        return;
    }
    const QString query = m_searchInput->text().trimmed();
    int count = 0;
    for (const auto &account : m_allAccounts) {
        if (!query.isEmpty() && !account.username.contains(query, Qt::CaseInsensitive)
                && !account.childUsername.contains(query, Qt::CaseInsensitive)) continue;
        auto *card = new AccountCardWidget(account, m_scrollContent);
        connect(card, &AccountCardWidget::editRequested, this, &AccountPage::editAccountRequested);
        connect(card, &AccountCardWidget::deleteRequested, this, &AccountPage::deleteAccountRequested);
        m_listLayout->addWidget(card);
        ++count;
    }
    if (!count) {
        auto *state = new EmptyStateWidget("Không tìm thấy tài khoản", "Thử tên khác hoặc xóa tìm kiếm.",
                                          "Xóa tìm kiếm", m_scrollContent);
        connect(state, &EmptyStateWidget::actionClicked, m_searchInput, &QLineEdit::clear);
        m_listLayout->addWidget(state, 1);
    } else {
        m_listLayout->addStretch();
    }
}

void AccountPage::setupUi()
{
    setObjectName("accountPage");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto *title = new QLabel("Tài khoản", this);
    title->setProperty("role", "pageTitle");
    layout->addWidget(title);
    m_metrics = new QFrame(this);
    m_metrics->setProperty("role", "metricCard");
    auto *metrics = new QVBoxLayout(m_metrics);
    metrics->setContentsMargins(16, 14, 16, 14);
    metrics->setSpacing(4);
    m_totalAccountsVal = new QLabel("0", m_metrics);
    m_totalAccountsVal->setObjectName("accountCount");
    m_totalAccountsVal->setProperty("role", "metricValue");
    auto *caption = new QLabel("Tổng tài khoản", m_metrics);
    caption->setProperty("role", "metricTitle");
    metrics->addWidget(m_totalAccountsVal);
    metrics->addWidget(caption);
    layout->addWidget(m_metrics);
    m_toolbar = new QFrame(this);
    m_toolbar->setProperty("role", "toolbarCard");
    auto *toolbar = new QHBoxLayout(m_toolbar);
    toolbar->setContentsMargins(12, 12, 12, 12);
    toolbar->setSpacing(8);
    m_searchInput = new QLineEdit(m_toolbar);
    m_searchInput->setObjectName("accountSearch");
    m_searchInput->setProperty("role", "panelSearchInput");
    m_searchInput->setPlaceholderText("Tìm tài khoản");
    m_searchInput->setAccessibleName("Tìm tài khoản");
    m_searchInput->setClearButtonEnabled(true);
    m_searchInput->setFixedHeight(38);
    connect(m_searchInput, &QLineEdit::textChanged, this, &AccountPage::renderAccounts);
    toolbar->addWidget(m_searchInput, 1);
    auto *add = new QPushButton("Thêm tài khoản", m_toolbar);
    add->setObjectName("addAccountButton");
    add->setProperty("role", "primaryActionButton");
    add->setFixedHeight(38);
    add->setCursor(Qt::PointingHandCursor);
    connect(add, &QPushButton::clicked, this, &AccountPage::requestAddAccount);
    toolbar->addWidget(add);
    layout->addWidget(m_toolbar);
    auto *scroll = new QScrollArea(this);
    scroll->setProperty("role", "scrollArea");
    scroll->setWidgetResizable(true);
    m_scrollContent = new QWidget(scroll);
    m_scrollContent->setProperty("role", "scrollContent");
    m_scrollContent->setAttribute(Qt::WA_StyledBackground, true);
    m_listLayout = new QVBoxLayout(m_scrollContent);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(16);
    scroll->setWidget(m_scrollContent);
    layout->addWidget(scroll, 1);
}
