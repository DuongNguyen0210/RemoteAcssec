#include "SessionsPage.h"

#include <QFrame>
#include <QPainter>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

class ScreenViewer : public QWidget {
public:
    explicit ScreenViewer(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName("screenViewer");
        setMinimumSize(320, 240);
    }
    void setImage(const QImage &image) { m_image = image; update(); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(24, 26, 30));
        if (m_image.isNull()) {
            painter.setPen(Qt::white);
            painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Chưa có màn hình từ phiên kết nối"));
            return;
        }
        const QSize size = m_image.size().scaled(this->size(), Qt::KeepAspectRatio);
        const QRect target(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
        painter.drawImage(target, m_image);
    }
private:
    QImage m_image;
};


QLabel *label(const QString &text, const QString &objectName, QWidget *parent)
{
    QLabel *lbl = new QLabel(text, parent);
    lbl->setProperty("role", objectName);
    return lbl;
}

QFrame *metricCard(const QString &value, const QString &title, const QString &detail, QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setProperty("role", "metricCard");
    card->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(4);
    layout->addWidget(label(value, "metricValue", card));
    layout->addWidget(label(title, "metricTitle", card));
    layout->addWidget(label(detail, "metricDetail", card));
    return card;
}

QPushButton *actionButton(const QString &text, const QString &objectName, QWidget *parent)
{
    QPushButton *button = new QPushButton(text, parent);
    button->setProperty("role", objectName);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(38);
    return button;
}

QFrame *sessionCard(const QString &device, const QString &address, const QString &user,
                    const QString &duration, const QString &quality, QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setProperty("role", "sessionCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setMinimumHeight(190);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout();
    header->setSpacing(12);

    QLabel *icon = label("PC", "cardIcon", card);
    icon->setFixedSize(40, 40);
    icon->setAlignment(Qt::AlignCenter);
    header->addWidget(icon);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    titleLayout->addWidget(label(device, "cardTitle", card));
    titleLayout->addWidget(label(address, "cardSubtitle", card));
    header->addLayout(titleLayout, 1);

    QLabel *status = label("Đang kết nối", "stateChip", card);
    status->setProperty("state", "active");
    status->setAlignment(Qt::AlignCenter);
    header->addWidget(status);
    layout->addLayout(header);

    QGridLayout *details = new QGridLayout();
    details->setHorizontalSpacing(24);
    details->setVerticalSpacing(4);
    details->addWidget(label("NGƯỜI DÙNG", "metaLabel", card), 0, 0);
    details->addWidget(label("THỜI LƯỢNG", "metaLabel", card), 0, 1);
    details->addWidget(label("CHẤT LƯỢNG", "metaLabel", card), 0, 2);
    details->addWidget(label(user, "strongText", card), 1, 0);
    details->addWidget(label(duration, "strongText", card), 1, 1);
    details->addWidget(label(quality, "strongText", card), 1, 2);
    layout->addLayout(details);

    QFrame *line = new QFrame(card);
    line->setProperty("role", "thinDivider");
    line->setFrameShape(QFrame::HLine);
    layout->addWidget(line);

    QHBoxLayout *actions = new QHBoxLayout();
    actions->setSpacing(12);
    actions->addWidget(actionButton("Xem màn hình", "secondaryActionButton", card));
    actions->addWidget(actionButton("Ngắt kết nối", "dangerActionButton", card));
    layout->addLayout(actions);

    return card;
}

}

SessionsPage::SessionsPage(QWidget *parent)
    : QWidget{parent}
{
    setupUi();
}

void SessionsPage::setupUi()
{
    setObjectName("sessionsPage");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QVBoxLayout *header = new QVBoxLayout();
    header->setContentsMargins(0, 0, 0, 8);
    header->setSpacing(4);
    header->addWidget(label("Phiên kết nối", "pageTitle", this));
    header->addWidget(label("Các phiên đang kết nối.", "pageSubtitle", this));
    mainLayout->addLayout(header);

    QGridLayout *metrics = new QGridLayout();
    metrics->setHorizontalSpacing(16);
    metrics->setVerticalSpacing(16);
    metrics->addWidget(metricCard("—", "Phiên đang hoạt động", "Thông tin chưa có", this), 0, 0);
    metrics->addWidget(metricCard("—", "Chất lượng truyền", "Thông tin chưa có", this), 0, 1);
    metrics->addWidget(metricCard("—", "Thời lượng trung bình", "Thông tin chưa có", this), 0, 2);
    mainLayout->addLayout(metrics);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setProperty("role", "scrollArea");
    scrollArea->setWidgetResizable(true);

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setProperty("role", "scrollContent");
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout *sessions = new QVBoxLayout(scrollContent);
    sessions->setContentsMargins(0, 0, 0, 0);
    sessions->setSpacing(16);
    m_sessionCard = sessionCard("Phiên hiện tại", "—", "—", "—", "—", scrollContent);
    m_sessionCard->hide();
    sessions->addWidget(m_sessionCard);
    m_viewer = new ScreenViewer(scrollContent);
    sessions->addWidget(m_viewer, 1);
    sessions->addStretch();

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
    for (auto *button : findChildren<QPushButton*>()) {
        button->setEnabled(false);
        button->setText(button->text() + " · Sắp có");
        button->setToolTip("Sắp có");
    }

}

void SessionsPage::setFrame(const QImage &image)
{
    static_cast<ScreenViewer *>(m_viewer)->setImage(image);
}

void SessionsPage::clearFrame()
{
    static_cast<ScreenViewer *>(m_viewer)->setImage(QImage());
    m_sessionCard->hide();
}

void SessionsPage::showSession()
{
    clearFrame();
    m_sessionCard->show();
}
