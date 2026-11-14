#include "LogsPage.h"
#include "GUI/Components/EmptyStateWidget.h"

#include <QLabel>
#include <QVBoxLayout>

LogsPage::LogsPage(QWidget *parent)
    : QWidget{parent}
{
    setupUi();
}

void LogsPage::setupUi()
{
    setObjectName("logsPage");
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *title = new QLabel(tr("Nhật ký"), this);
    title->setProperty("role", "pageTitle");
    layout->addWidget(title);

    auto *emptyState = new EmptyStateWidget(tr("Nhật ký chưa khả dụng"),
        tr("Ứng dụng chưa cung cấp lịch sử hoạt động."), QString(), this);
    layout->addWidget(emptyState, 1);
}
