#include "topbarwidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QFile>

TopBarWidget::TopBarWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void TopBarWidget::setupUi()
{
    setObjectName("topbar");
    this->setAttribute(Qt::WA_StyledBackground, true);
    this->setAttribute(Qt::WA_Hover, true);

    QFile styleFile(":/styles/Resources/styles/topbar.qss");

    if (styleFile.open(QFile::ReadOnly))
    {
        QString styleSheet = QLatin1String(styleFile.readAll());
        this->setStyleSheet(styleSheet);
        styleFile.close();
    }

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 0, 20, 0);

    layout->addStretch();

    QHBoxLayout *BttLayout = new QHBoxLayout();
    BttLayout->setSpacing(20);
    QPushButton *NotifyBtt = new QPushButton(this);
    NotifyBtt->setIcon(QIcon(":/icons/Resources/icons/bell.svg"));
    QPushButton *HelpBtt = new QPushButton(this);
    HelpBtt->setIcon(QIcon(":/icons/Resources/icons/circle-question-mark.svg"));
    QPushButton *ProfileBtt = new QPushButton(this);
    ProfileBtt->setIcon(QIcon(":/icons/Resources/icons/circle-user.svg"));

    BttLayout->addWidget(NotifyBtt);
    BttLayout->addWidget(HelpBtt);
    BttLayout->addWidget(ProfileBtt);

    layout->addLayout(BttLayout);
}
