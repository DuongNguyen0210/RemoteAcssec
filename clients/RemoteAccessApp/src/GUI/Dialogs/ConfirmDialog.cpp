#include "ConfirmDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QShowEvent>
#include <QIcon>
#include <QPixmap>
#include <QStyle>

ConfirmDialog::ConfirmDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("confirmDialogWindow"));
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setupUI();
    setDialogType(Type::Question);
}

void ConfirmDialog::setupUI()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setAlignment(Qt::AlignCenter);

    m_cardFrame = new QFrame(this);
    m_cardFrame->setObjectName(QStringLiteral("confirmDialogCard"));
    m_cardFrame->setFixedWidth(400);

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(m_cardFrame);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(0, 0, 0, 45));
    shadow->setOffset(0, 6);
    m_cardFrame->setGraphicsEffect(shadow);

    QVBoxLayout *cardLayout = new QVBoxLayout(m_cardFrame);
    cardLayout->setContentsMargins(28, 28, 28, 24);
    cardLayout->setSpacing(16);
    cardLayout->setAlignment(Qt::AlignCenter);

    m_iconBadge = new QLabel(m_cardFrame);
    m_iconBadge->setObjectName(QStringLiteral("confirmBadge"));
    m_iconBadge->setFixedSize(54, 54);
    m_iconBadge->setAlignment(Qt::AlignCenter);

    m_titleLabel = new QLabel(m_cardFrame);
    m_titleLabel->setObjectName(QStringLiteral("confirmTitle"));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setWordWrap(true);

    m_messageLabel = new QLabel(m_cardFrame);
    m_messageLabel->setObjectName(QStringLiteral("confirmMessage"));
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setWordWrap(true);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->setContentsMargins(0, 8, 0, 0);

    m_cancelButton = new QPushButton(QStringLiteral("Hủy"), m_cardFrame);
    m_cancelButton->setObjectName(QStringLiteral("confirmBtnCancel"));
    m_cancelButton->setFixedHeight(38);
    m_cancelButton->setCursor(Qt::PointingHandCursor);

    m_confirmButton = new QPushButton(QStringLiteral("Xác nhận"), m_cardFrame);
    m_confirmButton->setObjectName(QStringLiteral("confirmBtnAction"));
    m_confirmButton->setFixedHeight(38);
    m_confirmButton->setCursor(Qt::PointingHandCursor);

    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_confirmButton);

    cardLayout->addWidget(m_iconBadge, 0, Qt::AlignCenter);
    cardLayout->addWidget(m_titleLabel);
    cardLayout->addWidget(m_messageLabel);
    cardLayout->addLayout(buttonLayout);

    rootLayout->addWidget(m_cardFrame);

    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_confirmButton, &QPushButton::clicked, this, &QDialog::accept);
}

void ConfirmDialog::setDialogType(Type type)
{
    m_type = type;
    updateVisuals();
}

void ConfirmDialog::updateVisuals()
{
    QString badgeType;
    QString actionRole;
    QString iconResource;

    switch (m_type) {
    case Type::Danger:
        badgeType = QStringLiteral("danger");
        actionRole = QStringLiteral("danger");
        iconResource = QStringLiteral(":/icons/Resources/icons/alert-circle.svg");
        break;
    case Type::Warning:
        badgeType = QStringLiteral("warning");
        actionRole = QStringLiteral("warning");
        iconResource = QStringLiteral(":/icons/Resources/icons/alert-triangle.svg");
        break;
    case Type::Info:
        badgeType = QStringLiteral("info");
        actionRole = QStringLiteral("primary");
        iconResource = QStringLiteral(":/icons/Resources/icons/check-circle.svg");
        break;
    case Type::Question:
    default:
        badgeType = QStringLiteral("question");
        actionRole = QStringLiteral("primary");
        iconResource = QStringLiteral(":/icons/Resources/icons/help-circle.svg");
        break;
    }

    m_iconBadge->setProperty("badgeType", badgeType);
    m_iconBadge->style()->unpolish(m_iconBadge);
    m_iconBadge->style()->polish(m_iconBadge);

    m_confirmButton->setProperty("actionRole", actionRole);
    m_confirmButton->style()->unpolish(m_confirmButton);
    m_confirmButton->style()->polish(m_confirmButton);

    QIcon icon(iconResource);
    m_iconBadge->setPixmap(icon.pixmap(QSize(28, 28)));
}

void ConfirmDialog::setTitleText(const QString &title)
{
    m_titleLabel->setText(title);
}

void ConfirmDialog::setMessageText(const QString &message)
{
    m_messageLabel->setText(message);
}

void ConfirmDialog::setConfirmButtonText(const QString &text)
{
    m_confirmButton->setText(text);
}

void ConfirmDialog::setCancelButtonText(const QString &text)
{
    m_cancelButton->setText(text);
}

void ConfirmDialog::setCancelButtonVisible(bool visible)
{
    m_cancelButton->setVisible(visible);
}

void ConfirmDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (parentWidget()) {
        QWidget *topWindow = parentWidget()->window();
        QPoint targetPos = topWindow->geometry().center() - rect().center();
        move(targetPos);
    }
    if (m_type == Type::Danger && m_cancelButton->isVisible()) {
        m_cancelButton->setFocus();
    } else {
        m_confirmButton->setFocus();
    }
}

void ConfirmDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_cancelButton->hasFocus()) {
            m_cancelButton->animateClick();
            return;
        }
        if (m_confirmButton->hasFocus()) {
            m_confirmButton->animateClick();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}

bool ConfirmDialog::confirmDelete(QWidget *parent,
                                  const QString &title,
                                  const QString &message,
                                  const QString &confirmText,
                                  const QString &cancelText)
{
    ConfirmDialog dialog(parent);
    dialog.setDialogType(Type::Danger);
    dialog.setTitleText(title);
    dialog.setMessageText(message);
    dialog.setConfirmButtonText(confirmText);
    dialog.setCancelButtonText(cancelText);
    dialog.setCancelButtonVisible(true);
    return dialog.exec() == QDialog::Accepted;
}

bool ConfirmDialog::confirm(QWidget *parent,
                            const QString &title,
                            const QString &message,
                            const QString &confirmText,
                            const QString &cancelText)
{
    ConfirmDialog dialog(parent);
    dialog.setDialogType(Type::Question);
    dialog.setTitleText(title);
    dialog.setMessageText(message);
    dialog.setConfirmButtonText(confirmText);
    dialog.setCancelButtonText(cancelText);
    dialog.setCancelButtonVisible(true);
    return dialog.exec() == QDialog::Accepted;
}

void ConfirmDialog::showWarning(QWidget *parent,
                                const QString &title,
                                const QString &message,
                                const QString &buttonText)
{
    ConfirmDialog dialog(parent);
    dialog.setDialogType(Type::Warning);
    dialog.setTitleText(title);
    dialog.setMessageText(message);
    dialog.setConfirmButtonText(buttonText);
    dialog.setCancelButtonVisible(false);
    dialog.exec();
}

void ConfirmDialog::showInfo(QWidget *parent,
                             const QString &title,
                             const QString &message,
                             const QString &buttonText)
{
    ConfirmDialog dialog(parent);
    dialog.setDialogType(Type::Info);
    dialog.setTitleText(title);
    dialog.setMessageText(message);
    dialog.setConfirmButtonText(buttonText);
    dialog.setCancelButtonVisible(false);
    dialog.exec();
}
