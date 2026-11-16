#include "AppCoordinator.h"

#include "AuthController.h"
#include "Network/Http/LoginSessionMonitor.h"
#include "Network/Http/ApiClient.h"
#include "GUI/Dialogs/ConfirmDialog.h"
#include "DevicesController.h"
#include "AccountController.h"
#include "SettingsController.h"
#include "GUI/Windows/MainWindow.h"
#include "GUI/Windows/ChildStatusWindow.h"
#include "GUI/Windows/RemoteDesktopWindow.h"
#include "GUI/Pages/SettingsPage.h"
#include "Network/Http/HeartbeatReporter.h"
#include "Streaming/ScreenStreamSender.h"
#include "Network/Relay/ChildSessionController.h"
#include "Network/Relay/StaticRelayEndpointProvider.h"
#include "Network/Relay/DynamicRelayEndpointProvider.h"
#include "Controllers/RemoteInputInjector.h"
#include "Domain/Store/DeviceStore.h"
#include "Domain/Store/AccountStore.h"
#include "Network/Http/AccountService.h"
#include "Network/Http/DeviceService.h"
#include "Network/Http/UserService.h"

#include <memory>
#include <QDebug>
#include <QGuiApplication>
#include <QApplication>
#include <QWidget>

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
    shutdownSession();
    if (m_remoteDesktopWindow) m_remoteDesktopWindow->deleteLater();
    if (m_childWindow) m_childWindow->deleteLater();
    if (m_mainWindow) m_mainWindow->deleteLater();
}

void AppCoordinator::shutdownSession()
{
    for (auto *child : findChildren<ChildSessionController *>()) child->stop();
    if (m_devicesController) m_devicesController->disconnectCurrentSession();
    if (m_heartbeatReporter) m_heartbeatReporter->stop();
    if (m_loginMonitor) m_loginMonitor->logout();
}

void AppCoordinator::start()
{
    m_authController = new AuthController(this);
    connect(m_authController, &AuthController::loginSuccess, this, &AppCoordinator::handleLoginSuccess);
    m_authController->start();
}

void AppCoordinator::handleLoginSuccess(const QString &role, const QString &username)
{
    if (!ApiClient::instance().getToken().isEmpty() && !m_loginMonitor) {
        m_loginMonitor = new LoginSessionMonitor(this);
        connect(qApp, &QCoreApplication::aboutToQuit, this, &AppCoordinator::shutdownSession);
        connect(m_loginMonitor, &LoginSessionMonitor::sessionLost, this, [this](const QString &message) {
            if (m_devicesController) m_devicesController->disconnectCurrentSession();
            for (auto *child : findChildren<ChildSessionController *>()) child->stop();
            if (m_heartbeatReporter) m_heartbeatReporter->stop();
            ConfirmDialog::showWarning(m_childWindow ? m_childWindow.data() : static_cast<QWidget *>(m_mainWindow.data()),
                                       QStringLiteral("Phiên đăng nhập đã kết thúc"), message);
            qApp->quit();
        });
        m_loginMonitor->start();
    }
    if (role == "ADMIN") createAdminWorkspace(username);
    else createChildWorkspace();

    if (m_authController) {
        m_authController->deleteLater();
    }
}

void AppCoordinator::createAdminWorkspace(const QString &username)
{
    m_deviceStore = new DeviceStore(this);
    m_accountStore = new AccountStore(this);

    auto *deviceService = new DeviceService(this);
    m_devicesController = new DevicesController(m_deviceStore, deviceService, this);
    m_accountController = new AccountController(m_accountStore, new AccountService(this), this);
    m_accountController->setOwnerUsername(username);

    auto *settingsPage = new SettingsPage();
    m_settingsController = new SettingsController(settingsPage, new UserService(this), this);
    m_settingsController->setKnownUsername(username);

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

void AppCoordinator::createChildWorkspace()
{
    QGuiApplication::setQuitOnLastWindowClosed(false);

    if (!m_heartbeatReporter)
    {
        m_heartbeatReporter = new HeartbeatReporter(this);
    }

    if (m_screenStreamSender) return;

    RelayEndpointProvider *provider = qEnvironmentVariable("REMOTE_RELAY_MODE", "static") == "dynamic"
            ? static_cast<RelayEndpointProvider *>(new DynamicRelayEndpointProvider(this))
            : static_cast<RelayEndpointProvider *>(new StaticRelayEndpointProvider(this));
    auto *session = new ChildSessionController(provider, this);
    auto injector = std::make_shared<RemoteInputInjector>();
    connect(session, &ChildSessionController::inputReceived, session,
            [injector](const Protocol::ProtocolHeader &header, const QByteArray &payload) {
                injector->inject(header.type, payload);
            });
    connect(session, &ChildSessionController::sessionEnded, session,
            [injector](const QString &) { injector->releaseAll(); });
    m_screenStreamSender = new ScreenStreamSender(session, this);
    connect(m_screenStreamSender, &ScreenStreamSender::frameCaptured, this,
            [injector](const QSize &size) { injector->setFrameSize(size); });
    session->setParent(m_screenStreamSender);
    provider->setParent(session);
    connect(m_heartbeatReporter, &HeartbeatReporter::authenticationLost,
            session, &ChildSessionController::stop);
    connect(session, &ChildSessionController::sessionFailed, this,
            [](const QString &reason) { qWarning() << "Child Relay:" << reason; });

    auto *window = new ChildStatusWindow;
    connect(session, &ChildSessionController::relayRegistered, window, &ChildStatusWindow::showRelayReady);
    connect(session, &ChildSessionController::sessionStarted, window, &ChildStatusWindow::showSessionStarted);
    connect(session, &ChildSessionController::sessionEnded, window, [window, session](const QString &) {
        window->showSessionEnded(session->relayReady());
    });
    connect(session, &ChildSessionController::sessionFailed, window, &ChildStatusWindow::showRelayFailure);
    connect(m_heartbeatReporter, &HeartbeatReporter::authenticationLost,
            window, &ChildStatusWindow::showAuthenticationLost);
    connect(window, &ChildStatusWindow::sharingToggleRequested, window, [this, session, window] {
        const bool enable = !m_heartbeatReporter->isRunning();
        if (enable) {
            m_heartbeatReporter->start();
            session->start();
        } else {
            session->stop();
            m_heartbeatReporter->stop();
        }
        window->showSharingEnabled(enable);
    });
    connect(window, &QObject::destroyed, session, &ChildSessionController::stop);
    connect(window, &QObject::destroyed, m_heartbeatReporter, &HeartbeatReporter::stop);
    connect(window, &QObject::destroyed, qApp, &QApplication::quit);
    m_childWindow = window;
    window->show();

}

void AppCoordinator::handlePageSelected(int pageIndex)
{
    if (pageIndex == 0 && m_devicesController) m_devicesController->refresh();
    if (pageIndex == 2 && m_settingsController) m_settingsController->loadSettings();
    if (pageIndex == 4 && m_accountController) m_accountController->fetchAccounts();
}

void AppCoordinator::handleRemoteSessionStarted(quint64 sessionId, const QString &agentSessionId)
{
    if (m_mainWindow) {
        m_mainWindow->showRemoteSession();
        m_mainWindow->addSessionLog(QStringLiteral("Bắt đầu phiên điều khiển"), agentSessionId);
    }
    qDebug() << "[AppCoordinator] Remote session started for" << agentSessionId
             << "sessionId=" << sessionId;

    if (m_remoteDesktopWindow) {
        m_remoteDesktopWindow->closeForSessionEnd();
        m_remoteDesktopWindow->deleteLater();
    }
    m_remoteSessionId = sessionId;
    m_remoteDesktopWindow = new RemoteDesktopWindow(sessionId, agentSessionId);
    connect(m_remoteDesktopWindow, &RemoteDesktopWindow::remoteInput, this,
            [this, sessionId](Protocol::MessageType type, const QByteArray &payload) {
                if (m_remoteSessionId == sessionId && m_devicesController)
                    m_devicesController->sendInput(type, payload);
            });
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
    if (sessionId != m_remoteSessionId) return;
    if (m_mainWindow) m_mainWindow->addSessionLog(QStringLiteral("Kết thúc phiên điều khiển"),
            reason.isEmpty() ? agentSessionId : reason);
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
