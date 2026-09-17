#include "CreateAccountDialog.h"
#include "PasswordField.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

CreateAccountDialog::CreateAccountDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName("createAccountDialog");
    setWindowTitle("Thêm tài khoản");
    setWindowModality(Qt::WindowModal);
    resize(480, 340);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    auto *title = new QLabel("Thêm tài khoản", this);
    title->setProperty("role", "accountName");
    layout->addWidget(title);
    auto *nameLabel = new QLabel("Tên tài khoản", this);
    nameLabel->setProperty("role", "fieldLabel");
    m_name = new QLineEdit(this);
    m_name->setObjectName("createChildUsername");
    m_name->setProperty("role", "accountPassword");
    m_name->setMinimumHeight(38);
    m_name->setMaxLength(50);
    m_name->setAccessibleName("Tên tài khoản");
    nameLabel->setBuddy(m_name);
    layout->addWidget(nameLabel);
    layout->addWidget(m_name);
    auto *passwordLabel = new QLabel("Mật khẩu", this);
    passwordLabel->setProperty("role", "fieldLabel");
    auto *password = new PasswordField(this);
    m_password = password->input();
    m_password->setObjectName("createPassword");
    passwordLabel->setBuddy(m_password);
    layout->addWidget(passwordLabel);
    layout->addWidget(password);
    m_status = new QLabel(this);
    m_status->setProperty("role", "accountError");
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);
    auto *actions = new QHBoxLayout;
    actions->addStretch();
    m_cancel = new QPushButton("Hủy", this);
    m_cancel->setProperty("role", "secondaryActionButton");
    m_cancel->setAutoDefault(false);
    m_save = new QPushButton("Thêm tài khoản", this);
    m_save->setObjectName("createAccountSubmit");
    m_save->setProperty("role", "primaryActionButton");
    m_save->setDefault(true);
    for (auto *button : {m_cancel, m_save}) {
        button->setMinimumHeight(38);
        button->setCursor(Qt::PointingHandCursor);
        actions->addWidget(button);
    }
    layout->addLayout(actions);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_save, &QPushButton::clicked, this, [this]() {
        if (!m_save->isEnabled()) return;
        if (m_name->text().trimmed().isEmpty() || m_password->text().trimmed().isEmpty()) {
            showError("Nhập tên tài khoản và mật khẩu.");
            return;
        }
        setBusy(true);
        emit registerRequested(m_name->text().trimmed(), m_password->text());
    });
    connect(this, &QDialog::finished, this, [this]() { m_password->clear(); });
    m_name->setFocus();
}
void CreateAccountDialog::setBusy(bool busy)
{
    m_save->setEnabled(!busy);
    m_cancel->setEnabled(!busy);
    m_name->setEnabled(!busy);
    m_password->parentWidget()->setEnabled(!busy);
    m_save->setText(busy ? "Đang thêm…" : "Thêm tài khoản");
    m_status->hide();
}
void CreateAccountDialog::showError(const QString &message)
{
    setBusy(false);
    m_status->setText(message);
    m_status->show();
}
void CreateAccountDialog::reject()
{
    if (m_save->isEnabled()) QDialog::reject();
}
