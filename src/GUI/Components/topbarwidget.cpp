#include "topbarwidget.h"
#include <QHBoxLayout>
#include <QLabel>

TopBarWidget::TopBarWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void TopBarWidget::setupUi()
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 0, 20, 0);

    QLineEdit *searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Search devices, IP, or tags...");
    searchEdit->setFixedWidth(300);
    layout->addWidget(searchEdit);

    layout->addStretch();

    QHBoxLayout *BttLayout = new QHBoxLayout();
    BttLayout->setSpacing(20);
    QPushButton *NotifyBtt = new QPushButton(this);
    NotifyBtt->setIcon(QIcon(":/icons/Resources/icons/bell.png"));
    QPushButton *HelpBtt = new QPushButton(this);
    HelpBtt->setIcon(QIcon(":/icons/Resources/icons/circle-question-mark.png"));
    QPushButton *ProfileBtt = new QPushButton(this);
    ProfileBtt->setIcon(QIcon(":/icons/Resources/icons/circle-user.png"));

    BttLayout->addWidget(NotifyBtt);
    BttLayout->addWidget(HelpBtt);
    BttLayout->addWidget(ProfileBtt);

    layout->addLayout(BttLayout);
}
