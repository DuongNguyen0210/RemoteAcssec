#ifndef APPCOORDINATOR_H
#define APPCOORDINATOR_H

#include <QObject>
#include <QPointer>
#include <QtGlobal>

class AuthController;
class MainWindow;
class HeartbeatReporter;
class ScreenStreamSender;
class DeviceStore;
class DevicesController;
class AccountController;

class AppCoordinator : public QObject
{
    Q_OBJECT
public:
    explicit AppCoordinator(QObject *parent = nullptr);
    ~AppCoordinator();

    void start();

private slots:
    void handleLoginSuccess(const QString &role, const QString &username);
    void handleRemoteSessionStarted(quint64 sessionId, const QString &childUsername);
    void handlePageSelected(int pageIndex);

private:
    QPointer<AuthController> m_authController;
    QPointer<MainWindow> m_mainWindow;
    HeartbeatReporter *m_heartbeatReporter;
    ScreenStreamSender *m_screenStreamSender;

    DeviceStore *m_deviceStore;
    DevicesController *m_devicesController;
    AccountController *m_accountController;
};

#endif
