#include "authcontroller.h"
#include "GUI/Windows/loginwindow.h"
#include "Core/Services/authservice.h"

AuthController::AuthController(QObject *parent)
    : QObject(parent),
      m_loginWindow(nullptr),
      m_authService(new AuthService(this))
{
    connect(m_authService, &AuthService::loginResult, this, &AuthController::handleAuthResult);
}

AuthController::~AuthController()
{
    if (m_loginWindow) m_loginWindow->deleteLater();
}

void AuthController::start()
{
    m_loginWindow = new LoginWindow();
    connect(m_loginWindow, &LoginWindow::loginRequested, this, &AuthController::handleLoginRequested);
    m_loginWindow->show();
}

void AuthController::handleLoginRequested(const QString &username, const QString &password)
{
    if (username.isEmpty() || password.isEmpty()) {
        m_loginWindow->showError("Vui lòng nhập đầy đủ thông tin!");
        return;
    }

    m_authService->login(username, password);
}

void AuthController::handleAuthResult(bool success, const QString &role, const QString &message,
                                      const QString &username)
{
    if (success)
    {
        emit loginSuccess(role, username);
        
        if (m_loginWindow)
            m_loginWindow->close();
    }
    else
        if (m_loginWindow)
            m_loginWindow->showError(message);
}
