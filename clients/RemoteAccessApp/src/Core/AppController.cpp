#include "AppController.h"

#include "AuthController.h"
#include "../GUI/Windows/MainWindow.h"
#include "RegisterController.h"
#include "Session/AdminSessionController.h"
#include "../Network/HeartbeatReporter.h"
#include "Screen/ScreenStreamSender.h"
#include "Store/DeviceStore.h"
#include "DevicesController.h"
#include "AccountController.h"

#include <QDebug>
#include <QGuiApplication>

AppController::AppController(QObject *parent)
    : QObject(parent),
      m_authController(nullptr),
      m_mainWindow(nullptr),
      m_registerController(nullptr),
      m_heartbeatReporter(nullptr),
      m_screenStreamSender(nullptr),
      m_adminSessionController(nullptr),
      m_deviceStore(nullptr),
      m_devicesController(nullptr),
      m_accountController(nullptr)
{
}

AppController::~AppController()
{
    if (m_authController) m_authController->deleteLater();
    if (m_mainWindow) m_mainWindow->deleteLater();
    if (m_registerController) m_registerController->deleteLater();
    if (m_heartbeatReporter) m_heartbeatReporter->deleteLater();
    if (m_screenStreamSender) m_screenStreamSender->deleteLater();
    if (m_adminSessionController) m_adminSessionController->deleteLater();
    if (m_devicesController) m_devicesController->deleteLater();
    if (m_accountController) m_accountController->deleteLater();
    if (m_deviceStore) m_deviceStore->deleteLater();
}

void AppController::start()
{
    m_authController = new AuthController(this);
    connect(m_authController, &AuthController::loginSuccess, this, &AppController::handleLoginSuccess);
    m_authController->start();
}

void AppController::handleLoginSuccess(const QString &role, const QString &username)
{
    if (role == "ADMIN")
    {
        m_deviceStore = new DeviceStore(this);
        m_devicesController = new DevicesController(m_deviceStore, this);
        m_accountController = new AccountController(m_deviceStore, this);

        m_mainWindow = new MainWindow(m_devicesController->getView(), m_accountController->getView());

        connect(m_mainWindow, &MainWindow::requestAddAccount, this, &AppController::handleRequestAddAccount);
        connect(m_mainWindow, &MainWindow::childConnectRequested,
                this, &AppController::handleChildConnectRequested);
        connect(m_mainWindow, &MainWindow::pageSelected,
                this, &AppController::handlePageSelected);

        m_adminSessionController = new AdminSessionController(this);
        connect(m_adminSessionController, &AdminSessionController::sessionEstablished,
                this, &AppController::handleSessionEstablished);
        connect(m_adminSessionController, &AdminSessionController::sessionFailed,
                this, &AppController::handleSessionFailed);
        m_mainWindow->show();

        // Initial fetch to populate DeviceStore
        m_deviceStore->refresh();
    }
    else
    {
        qDebug() << "CHILD account logged in";
        
        // Ensure application doesn't exit when AuthController windows are closed
        QGuiApplication::setQuitOnLastWindowClosed(false);

        // Start heartbeat reporter owned by AppController
        if (!m_heartbeatReporter) {
            m_heartbeatReporter = new HeartbeatReporter(this);
            m_heartbeatReporter->start();
        }

        // Start screen stream sender owned by AppController
        if (!m_screenStreamSender) {
            m_screenStreamSender = new ScreenStreamSender(username, this);
            m_screenStreamSender->start();
        }
    }

    if (m_authController)
        m_authController->deleteLater();
}

void AppController::handleRequestAddAccount()
{
    if (m_registerController)
        return;
    
    m_registerController = new RegisterController(this);
    connect(m_registerController, &RegisterController::accountCreatedSuccessfully, this, [this]() {
        if (m_deviceStore) {
            m_deviceStore->refresh();
        }
    });
    
    m_registerController->start("admin");
}

void AppController::handlePageSelected(int pageIndex)
{
    // Auto-refresh DeviceStore when navigating to Devices (0) or Accounts (4)
    if (pageIndex == 0 || pageIndex == 4) {
        if (m_deviceStore) {
            m_deviceStore->refresh();
        }
    }
}

void AppController::handleChildConnectRequested(const QString &childUsername)
{
    qDebug() << "[AppController] ADMIN da chon CHILD:" << childUsername;
    m_adminSessionController->requestSession(childUsername);
}

void AppController::handleSessionEstablished(quint64 sessionId)
{
    qDebug() << "[AppController] Phien Relay da ACTIVE, sessionId=" << sessionId;
}

void AppController::handleSessionFailed(const QString &reason)
{
    qWarning() << "[AppController] Tao phien Relay that bai:" << reason;
}
