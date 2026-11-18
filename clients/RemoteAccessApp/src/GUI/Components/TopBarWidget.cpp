#include "TopBarWidget.h"

#include <QHBoxLayout>
#include <QLineEdit>

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
    searchInput->setProperty("role", "panelSearchInput");
    searchInput->setPlaceholderText("Tìm thiết bị");
    searchInput->setAccessibleName("Tìm thiết bị");
    searchInput->setClearButtonEnabled(true);
    searchInput->setMinimumWidth(280);
    searchInput->setMaximumWidth(440);
    searchInput->setFixedHeight(40);
    connect(searchInput, &QLineEdit::textChanged, this, &TopBarWidget::searchChanged);
    layout->addWidget(searchInput, 1, Qt::AlignVCenter);
}

void TopBarWidget::clearSearch()
{
    searchInput->clear();
}
