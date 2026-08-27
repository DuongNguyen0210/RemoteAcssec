#include "loginwindow.h"
//#include "../Network/relayclient.h"
#include <QFrame>

#include <QDebug>

LoginWindow::LoginWindow(QWidget *parent) : QWidget(parent)
{
    setObjectName("loginWindow");
    setFixedSize(1080, 600);
    setWindowTitle("Remote Access - Đăng Nhập");
    
    // RelayClient* client = new RelayClient(this);
    // client->ConnectToServer("0.tcp.ap.ngrok.io", 29856);

    auth = new Authservice(this);
    connect(auth, &Authservice::loginResult, this, &LoginWindow::handleLoginResult);

    setupUi();
}

LoginWindow::~LoginWindow() {}

void LoginWindow::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);
    
    QFrame* loginBox = new QFrame(this);
    loginBox->setObjectName("loginBox");
    QVBoxLayout* boxLayout = new QVBoxLayout(loginBox);
    boxLayout->setSpacing(20);
    boxLayout->setContentsMargins(40, 40, 40, 40);

    QLabel* titleLabel = new QLabel("ĐĂNG NHẬP", loginBox);
    titleLabel->setObjectName("loginTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    
    usernameInput = new QLineEdit(loginBox);
    usernameInput->setObjectName("usernameInput");
    usernameInput->setPlaceholderText("Tên đăng nhập...");
    usernameInput->setFixedHeight(40);
    
    passwordInput = new QLineEdit(loginBox);
    passwordInput->setObjectName("passwordInput");
    passwordInput->setPlaceholderText("Mật khẩu...");
    passwordInput->setEchoMode(QLineEdit::Password);
    passwordInput->setFixedHeight(40);
    
    loginButton = new QPushButton("Đăng Nhập", loginBox);
    loginButton->setObjectName("loginButton");
    loginButton->setFixedHeight(45);
    loginButton->setCursor(Qt::PointingHandCursor);

    errorLabel = new QLabel("", loginBox);
    errorLabel->setObjectName("errorLabel");
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
}

void LoginWindow::handleLoginResult(bool Success, const QString &Role, const QString &Message)
{
    if(Success)
        emit loginSuccessful(true, (Role == "ADMIN"), Message);
    else
    {
        errorLabel->setText(Message);
        errorLabel->show();
    }

}


void LoginWindow::onLoginClicked()
{
    QString username = usernameInput->text();
    QString password = passwordInput->text();
    auth->login(username, password);
}
