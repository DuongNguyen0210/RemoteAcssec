#include "appcontroller.h"

#include "authcontroller.h"
#include "../GUI/Windows/mainwindow.h"
#include "registercontroller.h"

#include <QDebug>

AppController::AppController(QObject *parent)
    : QObject(parent),
      m_authController(nullptr),
      m_mainWindow(nullptr),
      m_registerController(nullptr)
{
}

AppController::~AppController()
{
    if (m_authController) m_authController->deleteLater();
    if (m_mainWindow) m_mainWindow->deleteLater();
    if (m_registerController) m_registerController->deleteLater();
}

void AppController::start()
{
    m_authController = new AuthController(this);
    connect(m_authController, &AuthController::loginSuccess, this, &AppController::handleLoginSuccess);
    m_authController->start();
}

void AppController::handleLoginSuccess(const QString &role)
{
    if (role == "ADMIN")
    {
        m_mainWindow = new MainWindow();

        connect(m_mainWindow, &MainWindow::requestAddAccount, this, &AppController::handleRequestAddAccount);
        m_mainWindow->show();
    }
    else
        qDebug() << "Sub-account logged in";


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
