#include "AccountCardWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

namespace {

QLabel *createLabel(const QString &text, const QString &objectName, QWidget *parent)
{
    QLabel *lbl = new QLabel(text, parent);
    lbl->setProperty("role", objectName);
    return lbl;
}

QPushButton *createButton(const QString &text, const QString &objectName, QWidget *parent)
{
    QPushButton *button = new QPushButton(text, parent);
    button->setProperty("role", objectName);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(36);
    return button;
}

}

AccountCardWidget::AccountCardWidget(const DeviceInfo &info, QWidget *parent)
    : QFrame(parent)
{
    const QString username = info.childUsername.isEmpty() ? info.username : info.childUsername;
    const QString password = info.password.isEmpty() ? QStringLiteral("N/A") : info.password;
    const QString status = info.isOnline ? QStringLiteral("Active") : QStringLiteral("Offline");
    const QString statusState = info.isOnline ? QStringLiteral("active") : QStringLiteral("offline");

    initUi(username, password, QStringLiteral("Child"), status, statusState);
}

AccountCardWidget::AccountCardWidget(const QString &username, const QString &password,
                                     const QString &role, const QString &status,
                                     const QString &statusState, QWidget *parent)
    : QFrame(parent)
{
    initUi(username, password, role, status, statusState);
}

void AccountCardWidget::initUi(const QString &username, const QString &password,
                               const QString &role, const QString &status,
                               const QString &statusState)
{
    m_username = username;

    setProperty("role", "sessionCard");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout();
    header->setSpacing(12);

    QLabel *avatar = createLabel(QString(username.isEmpty() ? '?' : username[0]).toUpper(), "cardIcon", this);
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    header->addWidget(avatar);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    titleLayout->addWidget(createLabel(username, "cardTitle", this));

    QHBoxLayout *passwordLayout = new QHBoxLayout();
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->setSpacing(8);
    QLabel *passwordLabel = createLabel(QStringLiteral("••••••••"), "cardSubtitle", this);
    passwordLayout->addWidget(passwordLabel);

    QPushButton *togglePwdBtn = new QPushButton(QStringLiteral("Show"), this);
    togglePwdBtn->setCursor(Qt::PointingHandCursor);
    togglePwdBtn->setProperty("role", "linkButton");
    togglePwdBtn->setFixedWidth(40);
    passwordLayout->addWidget(togglePwdBtn);
    passwordLayout->addStretch();

    connect(togglePwdBtn, &QPushButton::clicked, this, [passwordLabel, togglePwdBtn, password]() {
        if (passwordLabel->text() == QStringLiteral("••••••••")) {
            passwordLabel->setText(password);
            togglePwdBtn->setText(QStringLiteral("Hide"));
        } else {
            passwordLabel->setText(QStringLiteral("••••••••"));
            togglePwdBtn->setText(QStringLiteral("Show"));
        }
    });

    titleLayout->addLayout(passwordLayout);
    header->addLayout(titleLayout, 1);

    QLabel *chip = createLabel(status, "stateChip", this);
    chip->setProperty("state", statusState);
    chip->setAlignment(Qt::AlignCenter);
    header->addWidget(chip);

    layout->addLayout(header);

    QGridLayout *details = new QGridLayout();
    details->setHorizontalSpacing(24);
    details->setVerticalSpacing(4);
    details->addWidget(createLabel(QStringLiteral("ROLE"), "metaLabel", this), 0, 0);
    details->addWidget(createLabel(QStringLiteral("STATUS"), "metaLabel", this), 0, 1);
    details->addWidget(createLabel(role, "strongText", this), 1, 0);
    details->addWidget(createLabel(status, "strongText", this), 1, 1);
    layout->addLayout(details);

    QFrame *divider = new QFrame(this);
    divider->setProperty("role", "thinDivider");
    divider->setFrameShape(QFrame::HLine);
    layout->addWidget(divider);

    QHBoxLayout *actions = new QHBoxLayout();
    actions->setSpacing(10);
    QPushButton *editBtn = createButton(QStringLiteral("Edit"), "secondaryActionButton", this);
    QPushButton *deleteBtn = createButton(QStringLiteral("Delete"), "dangerActionButton", this);
    actions->addWidget(editBtn);
    actions->addWidget(deleteBtn);
    actions->addStretch();
    layout->addLayout(actions);

    connect(editBtn, &QPushButton::clicked, this, [this]() {
        emit editRequested(m_username);
    });
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(m_username);
    });
}
