#include "EditAccountDialog.h"
#include "PasswordField.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

EditAccountDialog::EditAccountDialog(const AccountInfo &info, QWidget *parent) : QDialog(parent)
{
    setObjectName("editAccountDialog");
    setWindowTitle(QStringLiteral("Sửa tài khoản"));
    setWindowModality(Qt::WindowModal);
    resize(480, 420);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    auto *title = new QLabel(QStringLiteral("Sửa tài khoản"), this);
    title->setProperty("role", "accountName");
    layout->addWidget(title);
    auto field = [this, layout](const QString &name) {
        auto *label = new QLabel(name, this);
        label->setProperty("role", "accountFieldLabel");
        auto *input = new QLineEdit(this);
        input->setProperty("role", "accountPassword");
        input->setMinimumHeight(38);
        input->setAccessibleName(name);
        label->setBuddy(input);
        layout->addWidget(label);
        layout->addWidget(input);
        return input;
    };
    m_name = field(QStringLiteral("Tên tài khoản"));
    m_name->setObjectName("editChildUsername");
    const QString prefix = info.username.endsWith(info.childUsername)
        ? info.username.left(info.username.size() - info.childUsername.size()) : QString();
    m_name->setMaxLength(qMax(1, 50 - int(prefix.size())));
    m_name->setText(info.childUsername);
    auto *username = field(QStringLiteral("Tên đăng nhập"));
    username->setReadOnly(true);
    username->setText(info.username);
    connect(m_name, &QLineEdit::textChanged, this, [username, prefix](const QString &name) {
        username->setText(prefix + name.trimmed());
    });
    auto *passwordLabel = new QLabel("Mật khẩu mới", this);
    passwordLabel->setProperty("role", "fieldLabel");
    auto *password = new PasswordField(this);
    m_password = password->input();
    m_password->setObjectName("editNewPassword");
    m_password->setPlaceholderText("Để trống để giữ mật khẩu cũ");
    passwordLabel->setBuddy(m_password);
    layout->addWidget(passwordLabel);
    layout->addWidget(password);
    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setProperty("role", "accountError");
    m_status->hide();
    layout->addWidget(m_status);
    auto *actions = new QHBoxLayout;
    actions->addStretch();
    m_cancel = new QPushButton(QStringLiteral("Hủy"), this);
    m_cancel->setProperty("role", "secondaryActionButton");
    m_cancel->setAutoDefault(false);
    m_save = new QPushButton(QStringLiteral("Lưu thay đổi"), this);
    m_save->setObjectName("saveAccountChanges");
    m_save->setProperty("role", "primaryActionButton");
    m_save->setDefault(true);
    for (auto *button : {m_cancel, m_save}) {
        button->setMinimumHeight(38);
        actions->addWidget(button);
    }
    layout->addLayout(actions);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_save, &QPushButton::clicked, this, [this, id = info.id]() {
        if (!m_save->isEnabled()) return;
        if (m_name->text().trimmed().isEmpty()
                || (!m_password->text().isEmpty() && m_password->text().trimmed().isEmpty())) {
            showError(QStringLiteral("Tên tài khoản và mật khẩu mới không được chỉ chứa khoảng trắng."));
            return;
        }
        m_save->setEnabled(false);
        m_name->setEnabled(false);
        m_password->parentWidget()->setEnabled(false);
        m_cancel->setEnabled(false);
        m_save->setText("Đang lưu…");
        m_status->hide();
        emit saveRequested(id, m_name->text().trimmed(), m_password->text());
    });
    connect(this, &QDialog::finished, this, [this]() { m_password->clear(); });
    m_name->setFocus();
}

void EditAccountDialog::showError(const QString &message)
{
    m_save->setEnabled(true);
    m_name->setEnabled(true);
    m_password->parentWidget()->setEnabled(true);
    m_cancel->setEnabled(true);
    m_save->setText("Lưu thay đổi");
    m_status->setText(message);
    m_status->show();
}

void EditAccountDialog::reject()
{
    if (!m_save->isEnabled()) return;
    QDialog::reject();
}
