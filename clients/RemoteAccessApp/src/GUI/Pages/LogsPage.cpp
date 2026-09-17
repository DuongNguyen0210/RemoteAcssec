#include "LogsPage.h"

#include <QFrame>
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

QPushButton *filterButton(const QString &text, bool active, QWidget *parent)
{
    QPushButton *button = new QPushButton(text, parent);
    button->setProperty("role", "filterButton");
    button->setCheckable(true);
    button->setChecked(active);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(36);
    return button;
}

QFrame *logRow(const QString &time, const QString &event, const QString &source,
               const QString &severity, const QString &state, QWidget *parent)
{
    QFrame *row = new QFrame(parent);
    row->setProperty("role", "logRow");
    row->setAttribute(Qt::WA_StyledBackground, true);

    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(16);

    QLabel *timeLabel = label(time, "logTime", row);
    timeLabel->setFixedWidth(96);
    layout->addWidget(timeLabel);

    QVBoxLayout *eventLayout = new QVBoxLayout();
    eventLayout->setSpacing(2);
    eventLayout->addWidget(label(event, "cardTitle", row));
    eventLayout->addWidget(label(source, "cardSubtitle", row));
    layout->addLayout(eventLayout, 1);

    QLabel *chip = label(severity, "stateChip", row);
    chip->setProperty("state", state);
    chip->setAlignment(Qt::AlignCenter);
    layout->addWidget(chip);

    return row;
}

}

LogsPage::LogsPage(QWidget *parent)
    : QWidget{parent}
{
    setupUi();
}

void LogsPage::setupUi()
{
    setObjectName("logsPage");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QVBoxLayout *header = new QVBoxLayout();
    header->setContentsMargins(0, 0, 0, 8);
    header->setSpacing(4);
    header->addWidget(label("Nhật ký", "pageTitle", this));
    header->addWidget(label("Lịch sử hoạt động của hệ thống.", "pageSubtitle", this));
    mainLayout->addLayout(header);

    QFrame *toolbar = new QFrame(this);
    toolbar->setProperty("role", "toolbarCard");
    toolbar->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 12, 12, 12);
    toolbarLayout->setSpacing(8);

    QLineEdit *search = new QLineEdit(toolbar);
    search->setProperty("role", "panelSearchInput");
    search->setPlaceholderText("Tìm nhật ký");
    search->setClearButtonEnabled(true);
    search->setEnabled(false);
    search->setPlaceholderText("Tìm nhật ký — Sắp có");
    search->setFixedHeight(38);
    toolbarLayout->addWidget(search, 1);
    toolbarLayout->addWidget(filterButton("Tất cả", true, toolbar));
    toolbarLayout->addWidget(filterButton("Cảnh báo", false, toolbar));
    toolbarLayout->addWidget(filterButton("Lỗi", false, toolbar));
    mainLayout->addWidget(toolbar);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setProperty("role", "scrollArea");
    scrollArea->setWidgetResizable(true);

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setProperty("role", "scrollContent");
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout *logs = new QVBoxLayout(scrollContent);
    logs->setContentsMargins(0, 0, 0, 0);
    logs->setSpacing(10);
    logs->addWidget(logRow("09:42:18", "Bắt đầu phiên kết nối", "SRV-APOLLO-01 — j.doe@company.com", "Thông tin", "active", scrollContent));
    logs->addWidget(logRow("09:39:07", "Kết nối yếu", "MBP-SARAH-DESIGN bị mất gói tin", "Cảnh báo", "warning", scrollContent));
    logs->addWidget(logRow("09:12:54", "Thiết bị trực tuyến", "WKSTN-DEV-04 tại 192.168.1.104", "Thông tin", "active", scrollContent));
    logs->addWidget(logRow("08:58:31", "Đăng nhập thất bại", "Đăng nhập quản trị từ 10.0.0.18", "Lỗi", "error", scrollContent));
    logs->addWidget(logRow("08:44:10", "Đã cập nhật cài đặt", "Thời gian chờ đổi thành 30 phút", "Thay đổi", "neutral", scrollContent));
    logs->addStretch();

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
    for (auto *button : findChildren<QPushButton*>()) {
        button->setEnabled(false);
        button->setText(button->text() + " · Sắp có");
        button->setToolTip("Sắp có");
    }

}
