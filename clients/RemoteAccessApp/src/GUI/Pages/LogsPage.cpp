#include "LogsPage.h"

#include <QFrame>
#include <QDateTime>
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
    lbl->setTextFormat(Qt::PlainText);
    lbl->setProperty("role", objectName);
    return lbl;
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
    header->addWidget(label("Hoạt động điều khiển trong lần chạy này · Không lưu lâu dài", "pageSubtitle", this));
    mainLayout->addLayout(header);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setProperty("role", "scrollArea");
    scrollArea->setWidgetResizable(true);

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setProperty("role", "scrollContent");
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);
    m_entries = new QVBoxLayout(scrollContent);
    m_entries->setContentsMargins(0, 0, 0, 0);
    m_entries->setSpacing(10);
    m_empty = label("Chưa có hoạt động điều khiển trong lần chạy này.", "pageSubtitle", scrollContent);
    m_empty->setAlignment(Qt::AlignCenter);
    m_entries->addWidget(m_empty, 1);

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
}

void LogsPage::addEntry(const QString &event, const QString &detail, bool warning)
{
    if (m_entryCount == 0) {
        m_empty->hide();
        m_entries->addStretch();
    }
    m_entries->insertWidget(0, logRow(QDateTime::currentDateTime().toString("HH:mm:ss"),
            event, detail, warning ? QStringLiteral("Cảnh báo") : QStringLiteral("Thông tin"),
            warning ? QStringLiteral("warning") : QStringLiteral("active"), this));
    if (++m_entryCount > 100) {
        auto *oldest = m_entries->itemAt(m_entryCount - 1)->widget();
        m_entries->removeWidget(oldest);
        oldest->deleteLater();
        --m_entryCount;
    }
}
