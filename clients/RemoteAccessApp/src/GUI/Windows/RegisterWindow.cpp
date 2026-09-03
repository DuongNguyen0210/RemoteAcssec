#include <QObject>
#include <QVBoxLayout>
#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "RegisterWindow.h"

RegisterWindow::RegisterWindow(QWidget * parent) : QWidget(parent) {
    setObjectName("registerWindow");
    setFixedSize(500, 400);
    setWindowTitle("Đăng ký tài khoản con");
    setupUI();
}

RegisterWindow::~RegisterWindow() {}

void RegisterWindow::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);

    QFrame *RegisterBox = new QFrame(this);
    RegisterBox->setObjectName("registerBox");

    QVBoxLayout *RegisterBoxLayout = new QVBoxLayout(RegisterBox);
    RegisterBoxLayout->setSpacing(20);
    RegisterBoxLayout->setContentsMargins(40, 40, 40, 40);

    QLabel *RegisterBoxTitle = new QLabel("Đăng ký tài khoản con", RegisterBox);
    RegisterBoxTitle->setObjectName("registerTitle");
    RegisterBoxTitle->setAlignment(Qt::AlignCenter);

    m_usernameInput = new QLineEdit(RegisterBox);
    m_usernameInput->setObjectName("registerUsernameInput");
    m_usernameInput->setPlaceholderText("Tên đăng nhập (Hậu tố)...");

    m_passwordInput = new QLineEdit(RegisterBox);
    m_passwordInput->setObjectName("registerPasswordInput");
    m_passwordInput->setPlaceholderText("Mật khẩu...");
    m_passwordInput->setEchoMode(QLineEdit::Password);

    m_statusLabel = new QLabel("", RegisterBox);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->hide();

    QPushButton *RegisterBtt = new QPushButton("Tạo tài khoản", RegisterBox);
    RegisterBtt->setObjectName("registerBtt");
    RegisterBtt->setCursor(Qt::PointingHandCursor);

    RegisterBoxLayout->addWidget(RegisterBoxTitle);
    RegisterBoxLayout->addSpacing(10);
    RegisterBoxLayout->addWidget(m_usernameInput);
    RegisterBoxLayout->addWidget(m_passwordInput);
    RegisterBoxLayout->addWidget(m_statusLabel);
    RegisterBoxLayout->addWidget(RegisterBtt);
    
    mainLayout->addWidget(RegisterBox);

    connect(RegisterBtt, &QPushButton::clicked, this, &RegisterWindow::onRegisterClicked);
}

void RegisterWindow::onRegisterClicked()
{
    m_statusLabel->hide();
    QString username = m_usernameInput->text();
    QString password = m_passwordInput->text();
    emit registerRequested(username, password);
}

void RegisterWindow::showError(const QString &msg)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet("color: red;");
    m_statusLabel->show();
}

void RegisterWindow::showSuccess(const QString &msg)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet("color: green;");
    m_statusLabel->show();
}
