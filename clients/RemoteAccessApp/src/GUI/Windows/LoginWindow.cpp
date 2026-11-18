#include "LoginWindow.h"
#include <QFrame>

LoginWindow::LoginWindow(QWidget *parent) : QWidget(parent)
{
    setObjectName("loginWindow");
    resize(800, 600);
    setMinimumSize(360, 360);
    setWindowTitle("RemoteAccess — Đăng nhập");

    setupUi();
}

LoginWindow::~LoginWindow() {}

void LoginWindow::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);

    QFrame* loginBox = new QFrame(this);
    loginBox->setObjectName("loginBox");
    loginBox->setMaximumWidth(440);
    QVBoxLayout* boxLayout = new QVBoxLayout(loginBox);
    boxLayout->setSpacing(20);
    boxLayout->setContentsMargins(40, 40, 40, 40);

    QLabel* titleLabel = new QLabel("Đăng nhập", loginBox);
    titleLabel->setObjectName("loginTitle");
    titleLabel->setAlignment(Qt::AlignCenter);

    usernameInput = new QLineEdit(loginBox);
    usernameInput->setObjectName("usernameInput");
    usernameInput->setProperty("role", "formInput");
    usernameInput->setPlaceholderText("Tên đăng nhập");
    usernameInput->setFixedHeight(40);

    passwordInput = new QLineEdit(loginBox);
    passwordInput->setObjectName("passwordInput");
    passwordInput->setProperty("role", "formInput");
    passwordInput->setPlaceholderText("Mật khẩu");
    passwordInput->setEchoMode(QLineEdit::Password);
    passwordInput->setFixedHeight(40);

    loginButton = new QPushButton("Đăng nhập", loginBox);
    loginButton->setObjectName("loginButton");
    loginButton->setProperty("role", "primaryActionButton");
    loginButton->setFixedHeight(45);
    loginButton->setCursor(Qt::PointingHandCursor);

    errorLabel = new QLabel("", loginBox);
    errorLabel->setObjectName("errorLabel");
    errorLabel->setTextFormat(Qt::PlainText);
    errorLabel->setWordWrap(true);
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->hide();

    boxLayout->addWidget(titleLabel);
    boxLayout->addSpacing(20);
    boxLayout->addWidget(usernameInput);
    boxLayout->addWidget(passwordInput);
    boxLayout->addWidget(errorLabel);
    boxLayout->addWidget(loginButton);

    mainLayout->addWidget(loginBox);

    connect(loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(usernameInput, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
    connect(passwordInput, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
}

void LoginWindow::showError(const QString &errorMessage)
{
    loginButton->setEnabled(true);
    loginButton->setText("Đăng nhập");
    errorLabel->setText(errorMessage);
    errorLabel->show();
}

void LoginWindow::onLoginClicked()
{
    if (!loginButton->isEnabled()) return;
    loginButton->setEnabled(false);
    loginButton->setText("Đang đăng nhập…");
    errorLabel->hide();

    QString username = usernameInput->text();
    QString password = passwordInput->text();
    emit loginRequested(username, password);
}
