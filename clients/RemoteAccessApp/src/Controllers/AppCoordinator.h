#ifndef APPCOORDINATOR_H
#define APPCOORDINATOR_H

#include <QObject>
#include <QPointer>
#include <QtGlobal>

class QImage;
class AuthController;
class MainWindow;
class RemoteDesktopWindow;
class HeartbeatReporter;
class ScreenStreamSender;
class DeviceStore;
class AccountStore;
class DevicesController;
class AccountController;
class AccountCsvReader;

class AppCoordinator : public QObject
{
    Q_OBJECT
public:
    explicit AppCoordinator(QObject *parent = nullptr);
    ~AppCoordinator();

    void start();

private slots:
    void handleLoginSuccess(const QString &role, const QString &username);
    void handleRemoteSessionStarted(quint64 sessionId, const QString &agentSessionId);
    void handleFrameReady(quint64 sessionId, const QImage &image);
    void handleSessionEnded(quint64 sessionId, const QString &agentSessionId, const QString &reason);
    void handleDisconnectRequested();
    void handlePageSelected(int pageIndex);

private:
    QPointer<AuthController> m_authController;
    QPointer<MainWindow> m_mainWindow;
    QPointer<RemoteDesktopWindow> m_remoteDesktopWindow;
    quint64 m_remoteSessionId = 0;
    HeartbeatReporter *m_heartbeatReporter;
    ScreenStreamSender *m_screenStreamSender;

    DeviceStore *m_deviceStore;
    AccountStore *m_accountStore;
    DevicesController *m_devicesController;
    AccountController *m_accountController;
};

#endif
