#include "SessionsPage.h"

#include <QFrame>
#include "GUI/Components/RemoteScreenWidget.h"
#include <QHBoxLayout>
#include <QLabel>
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

QPushButton *actionButton(const QString &text, const QString &objectName, QWidget *parent)
{
    QPushButton *button = new QPushButton(text, parent);
    button->setProperty("role", objectName);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(38);
    return button;
}

QFrame *sessionCard(QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setProperty("role", "sessionCard");
    card->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout();
    header->setSpacing(12);

    QLabel *icon = label("PC", "cardIcon", card);
    icon->setFixedSize(40, 40);
    icon->setAlignment(Qt::AlignCenter);
    header->addWidget(icon);

    header->addWidget(label("Phiên điều khiển hiện tại", "cardTitle", card), 1);

    QLabel *status = label("Đang hoạt động", "stateChip", card);
    status->setProperty("state", "active");
    status->setAlignment(Qt::AlignCenter);
    header->addWidget(status);
    layout->addLayout(header);

    QHBoxLayout *actions = new QHBoxLayout();
    actions->setSpacing(12);
    actions->addStretch();
    auto *disconnectButton = actionButton("Ngắt kết nối", "dangerActionButton", card);
    disconnectButton->setObjectName("disconnectSessionButton");
    actions->addWidget(disconnectButton);
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

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setProperty("role", "scrollArea");
    scrollArea->setWidgetResizable(true);

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setProperty("role", "scrollContent");
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout *sessions = new QVBoxLayout(scrollContent);
    sessions->setContentsMargins(0, 0, 0, 0);
    sessions->setSpacing(16);
    m_sessionCard = sessionCard(scrollContent);
    m_sessionCard->hide();
    sessions->addWidget(m_sessionCard);
    m_viewer = new RemoteScreenWidget(scrollContent);
    m_viewer->setObjectName("screenViewer");
    m_viewer->setMinimumSize(320, 240);
    m_viewer->hide();
    sessions->addWidget(m_viewer, 1);
    m_emptyLabel = label("Chưa có phiên điều khiển. Chọn một máy ở trang Thiết bị để bắt đầu.",
                         "pageSubtitle", scrollContent);
    m_emptyLabel->setObjectName("sessionsEmptyState");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    sessions->addWidget(m_emptyLabel, 1);

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
    connect(m_sessionCard->findChild<QPushButton *>("disconnectSessionButton"),
            &QPushButton::clicked, this, &SessionsPage::disconnectRequested);

}

void SessionsPage::setFrame(const QImage &image)
{
    if (image.isNull()) return;
    m_emptyLabel->hide();
    m_viewer->show();
    m_viewer->updateFrame(image);
}

void SessionsPage::clearFrame()
{
    m_viewer->clearFrame();
    m_sessionCard->hide();
    m_viewer->hide();
    m_emptyLabel->show();
}

void SessionsPage::showSession()
{
    clearFrame();
    m_sessionCard->show();
    m_emptyLabel->hide();
    m_viewer->show();
}
