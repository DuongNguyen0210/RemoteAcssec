#include "AccountPage.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include "GUI/Dialogs/ConfirmDialog.h"
#include "GUI/Components/EmptyStateWidget.h"
#include "GUI/Components/AccountCardWidget.h"

namespace {

QLabel *label(const QString &text, const QString &objectName, QWidget *parent)
{
    QLabel *lbl = new QLabel(text, parent);
    lbl->setProperty("role", objectName);
    return lbl;
}

}

AccountPage::AccountPage(QWidget *parent)
    : QWidget{parent}
    , m_totalAccountsVal(nullptr)
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

void AccountPage::updateAccountList(const QList<AccountInfo> &accounts)
{
    m_allAccounts = accounts;
    updateMetrics();
    renderAccounts(m_searchInput ? m_searchInput->text() : QString());
}

void AccountPage::updateMetrics()
{
    if (m_totalAccountsVal) m_totalAccountsVal->setText(QString::number(m_allAccounts.size()));
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
    QList<AccountInfo> filtered;
    for (const AccountInfo &acc : m_allAccounts) {
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

    for (const AccountInfo &acc : filtered) {
        AccountCardWidget *card = new AccountCardWidget(acc, m_scrollContent);
        connect(card, &AccountCardWidget::deleteRequested,
                this, &AccountPage::deleteAccountRequested);
        m_listLayout->addWidget(card);
    }

    m_listLayout->addStretch();
}

void AccountPage::showError(const QString &message)
{
    ConfirmDialog::showWarning(this, QStringLiteral("Lỗi"), message);
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
    header->addWidget(label("Manage sub-accounts linked to your administrator account. You can add or remove them below.",
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
