#include "AccountCardWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QIcon icon(const QString &name)
{
    return QIcon(":/icons/Resources/icons/account-" + name + ".svg");
}
}

AccountCardWidget::AccountCardWidget(const AccountInfo &info, QWidget *parent)
    : QFrame(parent)
{
    setProperty("role", "accountCard");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);
    auto label = [this](const QString &text, const char *role) {
        auto *widget = new QLabel(text, this);
        widget->setTextFormat(Qt::PlainText);
        widget->setProperty("role", role);
        return widget;
    };
    auto divider = [this, layout]() {
        auto *line = new QFrame(this);
        line->setProperty("role", "thinDivider");
        line->setFixedHeight(1);
        layout->addWidget(line);
    };

    auto *header = new QHBoxLayout;
    header->setSpacing(12);
    const QString name = info.childUsername.isEmpty() ? info.username : info.childUsername;
    auto *avatar = label(name.left(2).toUpper(), "accountAvatar");
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    header->addWidget(avatar);
    auto *identity = new QVBoxLayout;
    identity->setSpacing(4);
    identity->addWidget(label("TÊN TÀI KHOẢN", "metaLabel"));
    auto *title = label(name, "cardTitle");
    title->setWordWrap(true);
    title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    identity->addWidget(title);
    header->addLayout(identity, 1);
    layout->addLayout(header);
    divider();

    auto *details = new QHBoxLayout;
    details->setSpacing(20);
    auto *userColumn = new QVBoxLayout;
    userColumn->setSpacing(8);
    userColumn->addWidget(label("TÊN ĐĂNG NHẬP", "metaLabel"));
    auto *badge = new QFrame(this);
    badge->setProperty("role", "accountUsernameBadge");
    auto *badgeLayout = new QHBoxLayout(badge);
    badgeLayout->setContentsMargins(12, 8, 12, 8);
    badgeLayout->setSpacing(8);
    auto *userIcon = label({}, "cardSubtitle");
    userIcon->setPixmap(icon("user").pixmap(16, 16));
    badgeLayout->addWidget(userIcon);
    auto *username = label(info.username, "strongText");
    username->setWordWrap(true);
    username->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    badgeLayout->addWidget(username, 1);
    userColumn->addWidget(badge, 0, Qt::AlignLeft);
    auto *passwordColumn = new QVBoxLayout;
    passwordColumn->setSpacing(8);
    passwordColumn->addWidget(label("MẬT KHẨU", "metaLabel"));
    auto *passwordRow = new QHBoxLayout;
    passwordRow->setSpacing(8);
    auto *masked = label(QStringLiteral("• • • • • • • • • • • •"), "strongText");
    masked->setAccessibleName(QStringLiteral("Mật khẩu được bảo vệ; không thể xem lại"));
    passwordRow->addWidget(masked);
    auto *eye = new QToolButton(this);
    eye->setProperty("role", "accountEye");
    eye->setIcon(icon("eye"));
    eye->setEnabled(true);
    eye->setIconSize(QSize(16, 16));
    eye->setFixedSize(32, 36);
    eye->setAccessibleName(QStringLiteral("Không thể xem mật khẩu hiện tại"));
    const QString passwordHint = QStringLiteral("Chọn để xem mật khẩu.");
    masked->setToolTip(passwordHint);
    eye->setToolTip(passwordHint);
    passwordRow->addWidget(eye);
    passwordRow->addStretch();
    passwordColumn->addLayout(passwordRow);
    details->addLayout(userColumn, 1);
    details->addLayout(passwordColumn, 1);
    layout->addLayout(details);
    divider();

    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    actions->addStretch();
    auto button = [this, actions](const QString &text, const char *role, const QString &iconName) {
        auto *widget = new QPushButton(icon(iconName), text, this);
        widget->setProperty("role", role);
        widget->setIconSize(QSize(16, 16));
        widget->setFixedHeight(38);
        widget->setMinimumWidth(96);
        widget->setCursor(Qt::PointingHandCursor);
        actions->addWidget(widget);
        return widget;
    };
    auto *edit = button("Sửa", "primaryActionButton", "edit");
    auto *remove = button("Xóa", "dangerActionButton", "delete");
    edit->setObjectName("editAccountButton");
    remove->setObjectName("deleteAccountButton");
    edit->setAccessibleName(QStringLiteral("Sửa tài khoản %1").arg(info.username));
    remove->setAccessibleName(QStringLiteral("Xóa tài khoản %1").arg(info.username));
    layout->addLayout(actions);
    connect(edit, &QPushButton::clicked, this, [this, username = info.username]() {
        emit editRequested(username);
    });
    connect(remove, &QPushButton::clicked, this, [this, username = info.username]() {
        emit deleteRequested(username);
    });
}
