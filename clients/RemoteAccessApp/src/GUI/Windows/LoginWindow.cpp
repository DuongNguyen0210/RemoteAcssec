#include "LoginWindow.h"
#include <QFrame>
#include <QCheckBox>
#include <QRegularExpression>

namespace {
constexpr int authInputHeight = 40;
constexpr int authButtonHeight = 45;
}

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
    loginForm = loginBox;
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
    usernameInput->setFixedHeight(authInputHeight);

    passwordInput = new QLineEdit(loginBox);
    passwordInput->setObjectName("passwordInput");
    passwordInput->setProperty("role", "formInput");
    passwordInput->setPlaceholderText("Mật khẩu");
    passwordInput->setEchoMode(QLineEdit::Password);
    passwordInput->setFixedHeight(authInputHeight);

    loginButton = new QPushButton("Đăng nhập", loginBox);
    loginButton->setObjectName("loginButton");
    loginButton->setProperty("role", "primaryActionButton");
    loginButton->setFixedHeight(authButtonHeight);
    loginButton->setCursor(Qt::PointingHandCursor);

    errorLabel = new QLabel("", loginBox);
    errorLabel->setObjectName("errorLabel");
    errorLabel->setTextFormat(Qt::PlainText);
    errorLabel->setWordWrap(true);
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->hide();
    successLabel = new QLabel(loginBox);
    successLabel->setObjectName("registrationSuccess");
    successLabel->setProperty("role", "pageSubtitle");
    successLabel->setAlignment(Qt::AlignCenter);
    successLabel->setWordWrap(true);
    successLabel->hide();

    auto *registrationPrompt = new QHBoxLayout;
    registrationPrompt->setAlignment(Qt::AlignCenter);
    auto *promptLabel = new QLabel(QStringLiteral("Chưa có tài khoản?"), loginBox);
    promptLabel->setProperty("role", "pageSubtitle");
    registrationPrompt->addWidget(promptLabel);
    registrationLink = new QPushButton(QStringLiteral("Đăng ký ngay"), loginBox);
    registrationLink->setObjectName("openRegistrationButton");
    registrationLink->setProperty("role", "secondaryActionButton");
    registrationPrompt->addWidget(registrationLink);

    boxLayout->addWidget(titleLabel);
    boxLayout->addSpacing(20);
    boxLayout->addWidget(usernameInput);
    boxLayout->addWidget(passwordInput);
    boxLayout->addWidget(errorLabel);
    boxLayout->addWidget(successLabel);
    boxLayout->addWidget(loginButton);
    boxLayout->addLayout(registrationPrompt);

    mainLayout->addWidget(loginBox);
    setupRegistration(mainLayout);
    connect(registrationLink, &QPushButton::clicked, this, [this] {
        registrationName->setText(usernameInput->text().trimmed());
        passwordInput->clear();
        loginForm->hide();
        registrationForm->show();
        setWindowTitle(QStringLiteral("RemoteAccess — Đăng ký tài khoản"));
        registrationName->setFocus();
    });

    connect(loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(usernameInput, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
    connect(passwordInput, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
}

void LoginWindow::showError(const QString &errorMessage)
{
    registrationLink->setEnabled(true);
    loginButton->setEnabled(true);
    loginButton->setText("Đăng nhập");
    errorLabel->setText(errorMessage);
    errorLabel->show();
}

void LoginWindow::onLoginClicked()
{
    if (!loginButton->isEnabled()) return;
    loginButton->setEnabled(false);
    registrationLink->setEnabled(false);
    loginButton->setText("Đang đăng nhập…");
    errorLabel->hide();
    successLabel->hide();

    QString username = usernameInput->text();
    QString password = passwordInput->text();
    emit loginRequested(username, password);
}

void LoginWindow::setupRegistration(QVBoxLayout *layout)
{
    auto *box = new QFrame(this);
    registrationForm = box;
    box->setObjectName("registrationBox");
    box->setMaximumWidth(440);
    auto *form = new QVBoxLayout(box);
    form->setContentsMargins(40, 32, 40, 32);
    form->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Đăng ký tài khoản"), box);
    title->setObjectName("loginTitle");
    title->setAlignment(Qt::AlignCenter);
    form->addWidget(title);

    auto addInput = [&](const QString &hint, const char *id, bool password) {
        auto *input = new QLineEdit(box);
        input->setObjectName(id);
        input->setAccessibleName(hint);
        input->setPlaceholderText(hint);
        input->setProperty("role", "formInput");
        input->setFixedHeight(authInputHeight);
        if (password) input->setEchoMode(QLineEdit::Password);
        form->addWidget(input);
        connect(input, &QLineEdit::returnPressed, this, &LoginWindow::submitRegistration);
        return input;
    };
    registrationName = addInput(QStringLiteral("Tên đăng nhập"), "registrationName", false);
    registrationPassword = addInput(QStringLiteral("Mật khẩu"), "registrationPassword", true);
    registrationConfirm = addInput(QStringLiteral("Xác nhận mật khẩu"), "registrationConfirm", true);

    showPasswords = new QCheckBox(QStringLiteral("Hiện mật khẩu"), box);
    showPasswords->setObjectName("registrationShowPasswords");
    showPasswords->setMinimumHeight(32);
    showPasswords->setCursor(Qt::PointingHandCursor);
    form->addWidget(showPasswords);
    connect(showPasswords, &QCheckBox::toggled, this, [this](bool shown) {
        const auto mode = shown ? QLineEdit::Normal : QLineEdit::Password;
        registrationPassword->setEchoMode(mode);
        registrationConfirm->setEchoMode(mode);
    });

    registrationError = new QLabel(box);
    registrationError->setObjectName("errorLabel");
    registrationError->setTextFormat(Qt::PlainText);
    registrationError->setAlignment(Qt::AlignCenter);
    registrationError->setWordWrap(true);
    registrationError->hide();
    form->addWidget(registrationError);

    registerButton = new QPushButton(QStringLiteral("Đăng ký"), box);
    registerButton->setObjectName("registerButton");
    registerButton->setProperty("role", "primaryActionButton");
    registerButton->setFixedHeight(authButtonHeight);
    registerButton->setCursor(Qt::PointingHandCursor);
    form->addWidget(registerButton);
    backButton = new QPushButton(QStringLiteral("Quay lại đăng nhập"), box);
    backButton->setObjectName("backToLoginButton");
    backButton->setProperty("role", "secondaryActionButton");
    backButton->setFixedHeight(authButtonHeight);
    backButton->setCursor(Qt::PointingHandCursor);
    form->addWidget(backButton);
    connect(registerButton, &QPushButton::clicked, this, &LoginWindow::submitRegistration);
    connect(backButton, &QPushButton::clicked, this, &LoginWindow::showLoginForm);
    layout->addWidget(box);
    box->hide();
}

void LoginWindow::submitRegistration()
{
    if (!registerButton->isEnabled() || !registrationForm->isVisible()) return;
    const auto username = registrationName->text().trimmed();
    const auto password = registrationPassword->text();
    const auto confirmation = registrationConfirm->text();
    static const QRegularExpression nameRule(QStringLiteral("\\A[A-Za-z0-9_.-]{3,48}\\z"));
    QString error;
    if (username.isEmpty() || password.trimmed().isEmpty() || confirmation.isEmpty())
        error = QStringLiteral("Vui lòng nhập đầy đủ thông tin.");
    else if (!nameRule.match(username).hasMatch())
        error = QStringLiteral("Tên đăng nhập phải có 3–48 ký tự: chữ không dấu, số, dấu chấm, gạch dưới hoặc gạch ngang.");
    else if (password.size() < 8 || password.size() > 255)
        error = QStringLiteral("Mật khẩu phải có từ 8 đến 255 ký tự.");
    else if (password != confirmation)
        error = QStringLiteral("Mật khẩu xác nhận không khớp.");
    if (!error.isEmpty()) {
        registrationError->setText(error);
        registrationError->show();
        return;
    }

    registrationError->hide();
    registrationForm->setEnabled(false);
    registerButton->setText(QStringLiteral("Đang đăng ký…"));
    emit registrationRequested(username, password);
}

void LoginWindow::showLoginForm()
{
    registrationForm->hide();
    registrationPassword->clear();
    registrationConfirm->clear();
    showPasswords->setChecked(false);
    registrationError->hide();
    errorLabel->hide();
    successLabel->hide();
    setWindowTitle(QStringLiteral("RemoteAccess — Đăng nhập"));
    loginForm->show();
    usernameInput->setFocus();
}

void LoginWindow::showRegistrationResult(bool success, const QString &message, const QString &username)
{
    registrationForm->setEnabled(true);
    registerButton->setText(QStringLiteral("Đăng ký"));
    if (!success) {
        registrationError->setText(message);
        registrationError->show();
        return;
    }
    showLoginForm();
    usernameInput->setText(username);
    successLabel->setText(QStringLiteral("Đăng ký tài khoản thành công"));
    successLabel->show();
    passwordInput->setFocus();
}
