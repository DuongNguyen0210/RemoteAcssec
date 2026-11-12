#include "AppCoordinator.h"

#include "AuthController.h"
#include "DevicesController.h"
#include "AccountController.h"
#include "SettingsController.h"
#include "GUI/Windows/MainWindow.h"
#include "GUI/Windows/RemoteDesktopWindow.h"
#include "GUI/Pages/SettingsPage.h"
#include "Network/Http/HeartbeatReporter.h"
#include "Streaming/ScreenStreamSender.h"
#include "Network/Relay/ChildSessionController.h"
#include "Network/Relay/StaticRelayEndpointProvider.h"
#include "Network/Relay/DynamicRelayEndpointProvider.h"
#include "Domain/Store/DeviceStore.h"
#include "Domain/Store/AccountStore.h"
#include "Network/Http/AccountService.h"
#include "Network/Http/DeviceService.h"
#include "Network/Http/UserService.h"

#include <QDebug>
#include <QGuiApplication>

AppCoordinator::AppCoordinator(QObject *parent)
    : QObject(parent)
    , m_authController(nullptr)
    , m_mainWindow(nullptr)
    , m_heartbeatReporter(nullptr)
    , m_screenStreamSender(nullptr)
    , m_deviceStore(nullptr)
    , m_accountStore(nullptr)
    , m_devicesController(nullptr)
    , m_accountController(nullptr)
    , m_settingsController(nullptr)
{
}

AppCoordinator::~AppCoordinator()
{
    if (m_remoteDesktopWindow) m_remoteDesktopWindow->deleteLater();
    if (m_authController) m_authController->deleteLater();
    if (m_mainWindow) m_mainWindow->deleteLater();
    if (m_heartbeatReporter) m_heartbeatReporter->deleteLater();
    if (m_screenStreamSender) m_screenStreamSender->deleteLater();
    if (m_devicesController) m_devicesController->deleteLater();
    if (m_accountController) m_accountController->deleteLater();
    if (m_settingsController) m_settingsController->deleteLater();
    if (m_deviceStore) m_deviceStore->deleteLater();
    if (m_accountStore) m_accountStore->deleteLater();
}

void AppCoordinator::start()
{
    m_authController = new AuthController(this);
    connect(m_authController, &AuthController::loginSuccess, this, &AppCoordinator::handleLoginSuccess);
    m_authController->start();
}

void AppCoordinator::handleLoginSuccess(const QString &role, const QString &username)
{
    if (role == "ADMIN")
    {
        m_deviceStore = new DeviceStore(this);
        m_accountStore = new AccountStore(this);

        auto *deviceService = new DeviceService(this);
        m_devicesController = new DevicesController(m_deviceStore, deviceService, this);
        m_accountController = new AccountController(m_accountStore, new AccountService(this), this);

        auto *settingsPage = new SettingsPage();
        m_settingsController = new SettingsController(settingsPage, new UserService(this), this);

        m_mainWindow = new MainWindow(m_devicesController->getView(), m_accountController->getView(), settingsPage);

        connect(m_devicesController, &DevicesController::remoteSessionStarted,
                this, &AppCoordinator::handleRemoteSessionStarted);
        connect(m_devicesController, &DevicesController::remoteFrameReady,
                this, &AppCoordinator::handleFrameReady);
        connect(m_devicesController, &DevicesController::sessionEnded,
                this, &AppCoordinator::handleSessionEnded);
        connect(m_mainWindow, &MainWindow::disconnectRequested,
                m_devicesController, &DevicesController::disconnectCurrentSession);
        connect(m_mainWindow, &MainWindow::pageSelected,
                this, &AppCoordinator::handlePageSelected);

        m_mainWindow->show();
        m_devicesController->refresh();

        m_accountController->fetchAccounts();
    }
    else
    {
        QGuiApplication::setQuitOnLastWindowClosed(false);

        if (!m_heartbeatReporter)
        {
            m_heartbeatReporter = new HeartbeatReporter(this);
            m_heartbeatReporter->start();
        }

        if (!m_screenStreamSender)
        {
            RelayEndpointProvider *provider = qEnvironmentVariable("REMOTE_RELAY_MODE", "static") == "dynamic"
                    ? static_cast<RelayEndpointProvider *>(new DynamicRelayEndpointProvider(this))
                    : static_cast<RelayEndpointProvider *>(new StaticRelayEndpointProvider(this));
            auto *session = new ChildSessionController(provider, this);
            m_screenStreamSender = new ScreenStreamSender(session, this);
            session->setParent(m_screenStreamSender);
            provider->setParent(session);
            connect(m_heartbeatReporter, &HeartbeatReporter::authenticationLost,
                    session, &ChildSessionController::stop);
            connect(session, &ChildSessionController::sessionFailed, this,
                    [](const QString &reason) { qWarning() << "Child Relay:" << reason; });
            session->start();
        }
    }

    if (m_authController) {
        m_authController->deleteLater();
    }
}

void AppCoordinator::handlePageSelected(int pageIndex)
{
    if (pageIndex == 0 && m_devicesController) m_devicesController->refresh();
    if (pageIndex == 2 && m_settingsController) m_settingsController->loadSettings();
    if (pageIndex == 4 && m_accountController) m_accountController->fetchAccounts();
}

void AppCoordinator::handleRemoteSessionStarted(quint64 sessionId, const QString &agentSessionId)
{
    if (m_mainWindow) m_mainWindow->showRemoteSession();
    qDebug() << "[AppCoordinator] Remote session started for" << agentSessionId
             << "sessionId=" << sessionId;

    if (m_remoteDesktopWindow) {
        m_remoteDesktopWindow->closeForSessionEnd();
        m_remoteDesktopWindow->deleteLater();
    }
    m_remoteSessionId = sessionId;
    m_remoteDesktopWindow = new RemoteDesktopWindow(sessionId, agentSessionId);
    connect(m_remoteDesktopWindow, &RemoteDesktopWindow::disconnectRequested, this, [this, sessionId] {
        if (m_remoteSessionId == sessionId) handleDisconnectRequested();
    });
    m_remoteDesktopWindow->show();
}

void AppCoordinator::handleFrameReady(quint64 sessionId, const QImage &image)
{
    if (sessionId != m_remoteSessionId || image.isNull()) return;
    if (m_mainWindow) m_mainWindow->setRemoteFrame(image);
    if (m_remoteDesktopWindow) m_remoteDesktopWindow->updateFrame(image);
}

void AppCoordinator::handleSessionEnded(quint64 sessionId, const QString &agentSessionId, const QString &reason)
{
    Q_UNUSED(agentSessionId);
    Q_UNUSED(reason);
    if (sessionId != m_remoteSessionId) return;
    m_remoteSessionId = 0;
    if (m_mainWindow) m_mainWindow->clearRemoteFrame();
    auto *window = m_remoteDesktopWindow.data();
    m_remoteDesktopWindow = nullptr;
    if (window) {
        window->closeForSessionEnd();
        window->deleteLater();
    }
}

void AppCoordinator::handleDisconnectRequested()
{
    if (m_devicesController) {
        m_devicesController->disconnectCurrentSession();
    }
}
