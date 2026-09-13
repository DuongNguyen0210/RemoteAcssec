#include "AppCoordinator.h"

#include "AuthController.h"
#include "DevicesController.h"
#include "AccountController.h"
#include "GUI/Windows/MainWindow.h"
#include "Network/Http/HeartbeatReporter.h"
#include "Streaming/ScreenStreamSender.h"
#include "Domain/Store/DeviceStore.h"

#include <QDebug>
#include <QGuiApplication>

AppCoordinator::AppCoordinator(QObject *parent)
    : QObject(parent)
    , m_authController(nullptr)
    , m_mainWindow(nullptr)
    , m_heartbeatReporter(nullptr)
    , m_screenStreamSender(nullptr)
    , m_deviceStore(nullptr)
    , m_devicesController(nullptr)
    , m_accountController(nullptr)
{
}

AppCoordinator::~AppCoordinator()
{
    if (m_authController) m_authController->deleteLater();
    if (m_mainWindow) m_mainWindow->deleteLater();
    if (m_heartbeatReporter) m_heartbeatReporter->deleteLater();
    if (m_screenStreamSender) m_screenStreamSender->deleteLater();
    if (m_devicesController) m_devicesController->deleteLater();
    if (m_accountController) m_accountController->deleteLater();
    if (m_deviceStore) m_deviceStore->deleteLater();
}

void AppCoordinator::start()
{
    m_authController = new AuthController(this);
    connect(m_authController, &AuthController::loginSuccess, this, &AppCoordinator::handleLoginSuccess);
    m_authController->start();
}

void AppCoordinator::handleLoginSuccess(const QString &role, const QString &username)
{
    if (role == "ADMIN") {
        m_deviceStore = new DeviceStore(this);
        m_devicesController = new DevicesController(m_deviceStore, this);
        m_accountController = new AccountController(m_deviceStore, this);

        m_mainWindow = new MainWindow(m_devicesController->getView(), m_accountController->getView());

        connect(m_devicesController, &DevicesController::remoteSessionStarted,
                this, &AppCoordinator::handleRemoteSessionStarted);
        connect(m_mainWindow, &MainWindow::pageSelected,
                this, &AppCoordinator::handlePageSelected);

        m_mainWindow->show();

        m_deviceStore->refresh();
    } else {
        qDebug() << "CHILD account logged in";

        QGuiApplication::setQuitOnLastWindowClosed(false);

        if (!m_heartbeatReporter) {
            m_heartbeatReporter = new HeartbeatReporter(this);
            m_heartbeatReporter->start();
        }

        if (!m_screenStreamSender) {
            m_screenStreamSender = new ScreenStreamSender(username, this);
            m_screenStreamSender->start();
        }
    }

    if (m_authController) {
        m_authController->deleteLater();
    }
}

void AppCoordinator::handlePageSelected(int pageIndex)
{
    if (pageIndex == 0 || pageIndex == 4) {
        if (m_deviceStore) {
            m_deviceStore->refresh();
        }
    }
}

void AppCoordinator::handleRemoteSessionStarted(quint64 sessionId, const QString &childUsername)
{
    qDebug() << "[AppCoordinator] Remote session started for" << childUsername
             << "sessionId=" << sessionId;
}
