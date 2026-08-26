#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <Authservice.h>

class LoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

signals:
    void loginSuccessful(bool Success, bool isAdmin, const QString &Message);

private slots:
    void onLoginClicked();
    void handleLoginResult(bool Success, const QString &Role, const QString &Message);

private:
    void setupUi();

    Authservice *auth;

    QLineEdit* usernameInput;
    QLineEdit* passwordInput;
    QPushButton* loginButton;
    QLabel* errorLabel;
};

#endif // LOGINWINDOW_H
