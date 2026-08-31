#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QPointer>

class AuthController;
class MainWindow;
class RegisterController;
class HeartbeatReporter;
class ScreenStreamSender;

class AppController : public QObject
{
    Q_OBJECT
public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController();

    void start();

private slots:
    void handleLoginSuccess(const QString &role, const QString &username);
    void handleRequestAddAccount();
    void handleChildConnectRequested(const QString &childUsername);

private:
    QPointer<AuthController> m_authController;
    QPointer<MainWindow> m_mainWindow;
    QPointer<RegisterController> m_registerController;
    HeartbeatReporter *m_heartbeatReporter;
    ScreenStreamSender *m_screenStreamSender;
};

#endif
