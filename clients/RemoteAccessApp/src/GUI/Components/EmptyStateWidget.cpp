#include "EmptyStateWidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

EmptyStateWidget::EmptyStateWidget(QWidget *parent)
    : QWidget(parent)
    , m_iconLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_descLabel(nullptr)
    , m_actionButton(nullptr)
{
    setupUi();
}

EmptyStateWidget::EmptyStateWidget(const QString &title, const QString &description,
                                   const QString &actionText, QWidget *parent)
    : QWidget(parent)
    , m_iconLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_descLabel(nullptr)
    , m_actionButton(nullptr)
{
    setupUi();
    setTitle(title);
    setDescription(description);
    setActionText(actionText);
}

void EmptyStateWidget::setupUi()
{
    setObjectName("emptyStateWidget");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 60, 40, 60);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignCenter);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setObjectName("emptyStateIcon");
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setText("📭");

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("emptyStateTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);

    m_descLabel = new QLabel(this);
    m_descLabel->setObjectName("emptyStateDesc");
    m_descLabel->setAlignment(Qt::AlignCenter);
    m_descLabel->setWordWrap(true);

    m_actionButton = new QPushButton(this);
    m_actionButton->setObjectName("emptyStateActionBtn");
    m_actionButton->setCursor(Qt::PointingHandCursor);
    m_actionButton->setFixedHeight(36);
    m_actionButton->hide();

    connect(m_actionButton, &QPushButton::clicked, this, &EmptyStateWidget::actionClicked);

    layout->addStretch();
    layout->addWidget(m_iconLabel);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_descLabel);
    layout->addSpacing(8);
    layout->addWidget(m_actionButton, 0, Qt::AlignCenter);
    layout->addStretch();
}

void EmptyStateWidget::setTitle(const QString &title)
{
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

void EmptyStateWidget::setDescription(const QString &description)
{
    if (m_descLabel) {
        m_descLabel->setText(description);
    }
}

void EmptyStateWidget::setActionText(const QString &actionText)
{
    if (!m_actionButton) return;

    if (actionText.trimmed().isEmpty()) {
        m_actionButton->hide();
    } else {
        m_actionButton->setText(actionText);
        m_actionButton->show();
    }
}

void EmptyStateWidget::setActionVisible(bool visible)
{
    if (m_actionButton) {
        m_actionButton->setVisible(visible);
    }
}

void EmptyStateWidget::setIconText(const QString &iconText)
{
    if (m_iconLabel) {
        m_iconLabel->setText(iconText);
    }
}
