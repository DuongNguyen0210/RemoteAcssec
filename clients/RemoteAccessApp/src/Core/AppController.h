#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QtGlobal>

class AuthController;
class MainWindow;
class RegisterController;
class HeartbeatReporter;
class ScreenStreamSender;
class AdminSessionController;
class DeviceStore;
class DevicesController;
class AccountController;

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
    void handleSessionEstablished(quint64 sessionId);
    void handleSessionFailed(const QString &reason);
    void handlePageSelected(int pageIndex);

private:
    QPointer<AuthController> m_authController;
    QPointer<MainWindow> m_mainWindow;
    QPointer<RegisterController> m_registerController;
    HeartbeatReporter *m_heartbeatReporter;
    ScreenStreamSender *m_screenStreamSender;
    AdminSessionController *m_adminSessionController;

    DeviceStore *m_deviceStore;
    DevicesController *m_devicesController;
    AccountController *m_accountController;
};

#endif
