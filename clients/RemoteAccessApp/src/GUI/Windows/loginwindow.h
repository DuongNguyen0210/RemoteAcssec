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

signals:
    void loginRequested(const QString &username, const QString &password);

private slots:
    void onLoginClicked();

private:
    void setupUi();

    QLineEdit* usernameInput;
    QLineEdit* passwordInput;
    QPushButton* loginButton;
    QLabel* errorLabel;
};

#endif // LOGINWINDOW_H
