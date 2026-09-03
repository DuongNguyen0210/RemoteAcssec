#include "AccountPage.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

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
    togglePwdBtn->setStyleSheet("background: transparent; color: #3498db; border: none; text-decoration: underline;");
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

#include "../../Core/AccountController.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>

AccountPage::AccountPage(QWidget *parent)
    : QWidget{parent},
      m_controller(nullptr),
      m_listLayout(nullptr),
      m_scrollContent(nullptr)
{
    setupUi();
}

void AccountPage::setController(AccountController *controller)
{
    m_controller = controller;
    // Tự động load dữ liệu khi đã set controller
    loadData();
}

void AccountPage::loadData()
{
    if (m_controller) {
        m_controller->fetchAccounts();
    }
}

void AccountPage::showLoading()
{
    if (!m_listLayout || !m_scrollContent) return;

    // Xóa các widget cũ
    QLayoutItem *child;
    while ((child = m_listLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
    
    // Thêm label "Đang tải dữ liệu..." (Dùng hàm Hide danh hoặc có thể dùng label() tạo sẵn)
    QLabel *lbl = new QLabel("Đang tải dữ liệu...", m_scrollContent);
    lbl->setProperty("role", "metaLabel");
    m_listLayout->addWidget(lbl);
    m_listLayout->addStretch();
}

void AccountPage::updateAccountList(const QJsonArray &children)
{
    if (!m_listLayout || !m_scrollContent) return;

    // Xóa loading hoặc widget cũ
    QLayoutItem *child;
    while ((child = m_listLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    // Render lại danh sách
    for (int i = 0; i < children.size(); ++i) {
        QJsonObject childObj = children[i].toObject();
        QString username = childObj["username"].toString();
        QString password = childObj["password"].toString();
        if (password.isEmpty()) password = "N/A";
        
        m_listLayout->addWidget(accountCard(username, password, "Child", "Active", "active", m_scrollContent));
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

    auto metricCard = [](const QString &value, const QString &title,
                         const QString &detail, QWidget *parent) -> QFrame *
    {
        QFrame *card = new QFrame(parent);
        card->setProperty("role", "metricCard");
        card->setAttribute(Qt::WA_StyledBackground, true);
        QVBoxLayout *l = new QVBoxLayout(card);
        l->setContentsMargins(16, 14, 16, 14);
        l->setSpacing(4);
        auto mk = [](const QString &t, const QString &n, QWidget *p) -> QLabel * {
            QLabel *lb = new QLabel(t, p);
            lb->setProperty("role", n);
            return lb;
        };
        l->addWidget(mk(value,  "metricValue",  card));
        l->addWidget(mk(title,  "metricTitle",  card));
        l->addWidget(mk(detail, "metricDetail", card));
        return card;
    };

    metrics->addWidget(metricCard("3",  "Total sub-accounts",  "Under your administrator account", this), 0, 0);
    metrics->addWidget(metricCard("2",  "Active accounts",     "Currently enabled",                this), 0, 1);
    metrics->addWidget(metricCard("1",  "Inactive accounts",   "Disabled or suspended",            this), 0, 2);
    mainLayout->addLayout(metrics);

    QFrame *toolbar = new QFrame(this);
    toolbar->setProperty("role", "toolbarCard");
    toolbar->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 12, 12, 12);
    toolbarLayout->setSpacing(8);

    QLineEdit *search = new QLineEdit(toolbar);
    search->setProperty("role", "panelSearchInput");
    search->setPlaceholderText("Search accounts...");
    search->setClearButtonEnabled(true);
    search->setFixedHeight(38);
    toolbarLayout->addWidget(search, 1);

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

    // Dữ liệu sẽ được load qua hàm onFetchListChildrenResult, nên tạm thời chỉ cần stretch hoặc Loading...
    m_listLayout->addWidget(label("Đang tải dữ liệu...", "metaLabel", m_scrollContent));
    m_listLayout->addStretch();

    scrollArea->setWidget(m_scrollContent);
    mainLayout->addWidget(scrollArea, 1);
}
