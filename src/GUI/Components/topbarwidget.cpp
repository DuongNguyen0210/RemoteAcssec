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
    layout->setContentsMargins(0, 0, 0, 0);

    QLineEdit *searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Search devices, IP, or tags...");
    searchEdit->setFixedWidth(300);
    layout->addWidget(searchEdit);

    layout->addStretch();

    QPushButton *NotifyBtt = new QPushButton("🔔", this);
    QPushButton *HelpBtt = new QPushButton("❓", this);
    QPushButton *ProfileBtt = new QPushButton("👤", this);
    layout->addWidget(NotifyBtt);
    layout->addWidget(HelpBtt);
    layout->addWidget(ProfileBtt);
}
