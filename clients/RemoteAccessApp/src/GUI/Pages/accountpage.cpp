#include "accountpage.h"

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

QFrame *accountCard(const QString &username, const QString &email, const QString &role,
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

    QLabel *avatar = label(QString(username[0]).toUpper(), "cardIcon", card);
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    header->addWidget(avatar);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    titleLayout->addWidget(label(username, "cardTitle", card));
    titleLayout->addWidget(label(email,    "cardSubtitle", card));
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
{
    setupUi();
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

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setProperty("role", "scrollContent");
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *listLayout = new QVBoxLayout(scrollContent);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(14);

    listLayout->addWidget(accountCard("nguyen.van.a",  "nguyen.van.a@company.com",  "Viewer",    "Active",   "active",  scrollContent));
    listLayout->addWidget(accountCard("tran.thi.b",    "tran.thi.b@company.com",    "Operator",  "Active",   "active",  scrollContent));
    listLayout->addWidget(accountCard("le.minh.c",     "le.minh.c@company.com",     "Viewer",    "Inactive", "neutral", scrollContent));
    listLayout->addStretch();

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
}
