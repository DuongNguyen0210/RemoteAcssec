#include "LogsPage.h"
#include "GUI/Dialogs/ConfirmDialog.h"

#include <QFrame>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

namespace {

QLabel *label(const QString &text, const QString &objectName, QWidget *parent)
{
    QLabel *lbl = new QLabel(text, parent);
    lbl->setTextFormat(Qt::PlainText);
    lbl->setProperty("role", objectName);
    return lbl;
}


QFrame *logRow(const QDateTime &time, const QString &event, const QString &source,
               bool warning, quint64 sessionId, QWidget *parent)
{
    QFrame *row = new QFrame(parent);
    row->setProperty("role", "logRow");
    row->setAttribute(Qt::WA_StyledBackground, true);

    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(16);

    QLabel *timeLabel = label(time.toString("HH:mm:ss"), "logTime", row);
    timeLabel->setFixedWidth(96);
    layout->addWidget(timeLabel);

    QVBoxLayout *eventLayout = new QVBoxLayout();
    eventLayout->setSpacing(2);
    eventLayout->addWidget(label(event, "cardTitle", row));
    eventLayout->addWidget(label(source, "cardSubtitle", row));
    layout->addLayout(eventLayout, 1);

    if (warning) {
        QLabel *chip = label(QStringLiteral("Cảnh báo"), "stateChip", row);
        chip->setProperty("state", "warning");
        chip->setAlignment(Qt::AlignCenter);
        layout->addWidget(chip);
    }

    auto *details = new QPushButton(QStringLiteral("Thông tin"), row);
    details->setObjectName("logDetailsButton");
    details->setProperty("role", "secondaryActionButton");
    layout->addWidget(details);
    QObject::connect(details, &QPushButton::clicked, row,
                     [parent, time, event, source, warning, sessionId] {
        QStringList fields{
            QStringLiteral("Hoạt động: %1").arg(event),
            QStringLiteral("Thời gian: %1").arg(time.toString("dd/MM/yyyy HH:mm:ss")),
            QStringLiteral("Mức độ: %1").arg(warning ? QStringLiteral("Cảnh báo")
                                                     : QStringLiteral("Thông tin"))
        };
        if (!source.trimmed().isEmpty())
            fields.append(QStringLiteral("Nội dung: %1").arg(source));
        if (sessionId != 0)
            fields.append(QStringLiteral("Mã phiên: %1").arg(sessionId));

        auto *dialog = new ConfirmDialog(parent);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setDialogType(ConfirmDialog::Type::Info);
        dialog->setTitleText(QStringLiteral("Chi tiết nhật ký"));
        dialog->setMessageText(fields.join(QLatin1Char('\n')));
        dialog->setConfirmButtonText(QStringLiteral("Đóng"));
        dialog->setCancelButtonVisible(false);
        dialog->show();
    });

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

void LogsPage::addEntry(const QString &event, const QString &detail, bool warning, quint64 sessionId)
{
    if (m_entryCount == 0) {
        m_empty->hide();
        m_entries->addStretch();
    }
    m_entries->insertWidget(0, logRow(QDateTime::currentDateTime(), event, detail, warning, sessionId, this));
    if (++m_entryCount > 100) {
        auto *oldest = m_entries->itemAt(m_entryCount - 1)->widget();
        m_entries->removeWidget(oldest);
        oldest->deleteLater();
        --m_entryCount;
    }
}
