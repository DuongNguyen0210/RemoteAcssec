#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

class LoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

    void showError(const QString &errorMessage);
    void showRegistrationResult(bool success, const QString &message, const QString &username);

signals:
    void loginRequested(const QString &username, const QString &password);
    void registrationRequested(const QString &username, const QString &password);

private slots:
    void onLoginClicked();

private:
    void setupUi();
    void setupRegistration(QVBoxLayout *layout);
    void showLoginForm();
    void submitRegistration();

    QLineEdit* usernameInput;
    QLineEdit* passwordInput;
    QPushButton* loginButton;
    QLabel* errorLabel;
    QLabel *successLabel = nullptr;
    QWidget *loginForm = nullptr;
    QWidget *registrationForm = nullptr;
    QPushButton *registrationLink = nullptr;
    QLineEdit *registrationName = nullptr;
    QLineEdit *registrationPassword = nullptr;
    QLineEdit *registrationConfirm = nullptr;
    QPushButton *registerButton = nullptr;
    QPushButton *backButton = nullptr;
    QLabel *registrationError = nullptr;
    class QCheckBox *showPasswords = nullptr;
};

#endif
