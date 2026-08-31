#include "appcontroller.h"

#include "authcontroller.h"
#include "../GUI/Windows/mainwindow.h"
#include "registercontroller.h"
#include "../Network/HeartbeatReporter.h"
#include "Screen/ScreenStreamSender.h"

#include <QDebug>
#include <QGuiApplication>

AppController::AppController(QObject *parent)
    : QObject(parent),
      m_authController(nullptr),
      m_mainWindow(nullptr),
      m_registerController(nullptr),
      m_heartbeatReporter(nullptr),
      m_screenStreamSender(nullptr)
{
}

AppController::~AppController()
{
    if (m_authController) m_authController->deleteLater();
    if (m_mainWindow) m_mainWindow->deleteLater();
    if (m_registerController) m_registerController->deleteLater();
    if (m_heartbeatReporter) m_heartbeatReporter->deleteLater();
    if (m_screenStreamSender) m_screenStreamSender->deleteLater();
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
        m_mainWindow = new MainWindow();

        connect(m_mainWindow, &MainWindow::requestAddAccount, this, &AppController::handleRequestAddAccount);
        connect(m_mainWindow, &MainWindow::childConnectRequested,
                this, &AppController::handleChildConnectRequested);
        m_mainWindow->show();
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
    
    m_registerController->start("admin");
}

void AppController::handleChildConnectRequested(const QString &childUsername)
{
    qDebug() << "[AppController] ADMIN da chon CHILD:" << childUsername;
}
