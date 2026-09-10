#ifndef AUTHCONTROLLER_H
#define AUTHCONTROLLER_H

#include <QObject>

class LoginWindow;
class AuthService;

class AuthController : public QObject
{
    Q_OBJECT
    
public:
    explicit AuthController(QObject *parent = nullptr);
    ~AuthController();

    void start();

signals:
    void loginSuccess(const QString &role, const QString &username);

private slots:
    void handleLoginRequested(const QString &username, const QString &password);
    void handleAuthResult(bool success, const QString &role, const QString &message,
                          const QString &username);

private:
    LoginWindow *m_loginWindow;
    AuthService *m_authService;
};

#endif // AUTHCONTROLLER_H
