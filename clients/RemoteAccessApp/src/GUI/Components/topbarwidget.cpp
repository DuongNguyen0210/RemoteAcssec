#include "topbarwidget.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QSize>

TopBarWidget::TopBarWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void TopBarWidget::setupUi()
{
    setObjectName("topbar");
    this->setAttribute(Qt::WA_StyledBackground, true);
    this->setAttribute(Qt::WA_Hover, true);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(24, 0, 24, 0);
    layout->setSpacing(16);

    searchInput = new QLineEdit(this);
    searchInput->setObjectName("topbarSearchInput");
    searchInput->setPlaceholderText("Search devices...");
    searchInput->setClearButtonEnabled(true);
    searchInput->setMinimumWidth(280);
    searchInput->setMaximumWidth(440);
    searchInput->setFixedHeight(40);
    layout->addWidget(searchInput, 1, Qt::AlignVCenter);

    QHBoxLayout *BttLayout = new QHBoxLayout();
    BttLayout->setSpacing(8);
    QPushButton *NotifyBtt = new QPushButton(this);
    NotifyBtt->setObjectName("topbarIconButton");
    NotifyBtt->setIcon(QIcon(":/icons/Resources/icons/bell.svg"));
    NotifyBtt->setIconSize(QSize(20, 20));
    QPushButton *HelpBtt = new QPushButton(this);
    HelpBtt->setObjectName("topbarIconButton");
    HelpBtt->setIcon(QIcon(":/icons/Resources/icons/circle-question-mark.svg"));
    HelpBtt->setIconSize(QSize(20, 20));
    QPushButton *ProfileBtt = new QPushButton(this);
    ProfileBtt->setObjectName("topbarIconButton");
    ProfileBtt->setIcon(QIcon(":/icons/Resources/icons/circle-user.svg"));
    ProfileBtt->setIconSize(QSize(20, 20));

    BttLayout->addWidget(NotifyBtt);
    BttLayout->addWidget(HelpBtt);
    BttLayout->addWidget(ProfileBtt);

    layout->addLayout(BttLayout);
}

void TopBarWidget::setSearchPlaceholder(const QString &placeholder)
{
    if (searchInput) {
        searchInput->setPlaceholderText(placeholder);
        searchInput->clear();
    }
}
