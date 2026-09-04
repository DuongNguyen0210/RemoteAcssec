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
<<<<<<<< HEAD:clients/RemoteAccessApp/src/Core/Controllers/appcontroller.h
class RemoteControlSubsystem;
========
class DeviceStore;
class DevicesController;
class AccountController;
>>>>>>>> origin/refactor-client-architecture-and-presence:clients/RemoteAccessApp/src/Core/AppController.h

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
<<<<<<<< HEAD:clients/RemoteAccessApp/src/Core/Controllers/appcontroller.h
    RemoteControlSubsystem *m_remoteControlSubsystem = nullptr;
========

    DeviceStore *m_deviceStore;
    DevicesController *m_devicesController;
    AccountController *m_accountController;
>>>>>>>> origin/refactor-client-architecture-and-presence:clients/RemoteAccessApp/src/Core/AppController.h
};

#endif
