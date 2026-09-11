#include "CreateAccountDialog.h"

#include <QVBoxLayout>
#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStyle>

CreateAccountDialog::CreateAccountDialog(QWidget *parent)
    : QDialog(parent)
    , m_usernameInput(nullptr)
    , m_passwordInput(nullptr)
    , m_statusLabel(nullptr)
{
    setObjectName("createAccountDialog");
    setFixedSize(500, 400);
    setWindowTitle("Tạo tài khoản con");
    setupUI();
}

CreateAccountDialog::~CreateAccountDialog()
{
}

void CreateAccountDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);

    QFrame *registerBox = new QFrame(this);
    registerBox->setObjectName("registerBox");

    QVBoxLayout *boxLayout = new QVBoxLayout(registerBox);
    boxLayout->setSpacing(20);
    boxLayout->setContentsMargins(40, 40, 40, 40);

    QLabel *boxTitle = new QLabel("Tạo tài khoản con", registerBox);
    boxTitle->setObjectName("registerTitle");
    boxTitle->setAlignment(Qt::AlignCenter);

    m_usernameInput = new QLineEdit(registerBox);
    m_usernameInput->setObjectName("registerUsernameInput");
    m_usernameInput->setPlaceholderText("Tên đăng nhập (Hậu tố)...");

    m_passwordInput = new QLineEdit(registerBox);
    m_passwordInput->setObjectName("registerPasswordInput");
    m_passwordInput->setPlaceholderText("Mật khẩu...");
    m_passwordInput->setEchoMode(QLineEdit::Password);

    m_statusLabel = new QLabel("", registerBox);
    m_statusLabel->setObjectName("registerStatusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->hide();

    QPushButton *registerBtn = new QPushButton("Tạo tài khoản", registerBox);
    registerBtn->setObjectName("registerBtt");
    registerBtn->setCursor(Qt::PointingHandCursor);

    boxLayout->addWidget(boxTitle);
    boxLayout->addSpacing(10);
    boxLayout->addWidget(m_usernameInput);
    boxLayout->addWidget(m_passwordInput);
    boxLayout->addWidget(m_statusLabel);
    boxLayout->addWidget(registerBtn);

    mainLayout->addWidget(registerBox);

    connect(registerBtn, &QPushButton::clicked, this, &CreateAccountDialog::onRegisterClicked);
}

void CreateAccountDialog::onRegisterClicked()
{
    m_statusLabel->hide();
    QString username = m_usernameInput->text();
    QString password = m_passwordInput->text();
    emit registerRequested(username, password);
}

void CreateAccountDialog::showError(const QString &msg)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setProperty("status", "error");
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    m_statusLabel->show();
}

void CreateAccountDialog::showSuccess(const QString &msg)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setProperty("status", "success");
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    m_statusLabel->show();
}
