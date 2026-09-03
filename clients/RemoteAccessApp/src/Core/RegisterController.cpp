#include "RegisterController.h"
#include "../GUI/Windows/RegisterWindow.h"
#include "../Network/AccountService.h"

RegisterController::RegisterController(QObject *parent)
    : QObject(parent),
      m_registerWindow(nullptr),
      m_accountService(new AccountService(this))
{
    connect(m_accountService, &AccountService::createAccountResult,
            this, &RegisterController::handleAccountCreated);
}

RegisterController::~RegisterController()
{
    if (m_registerWindow) m_registerWindow->deleteLater();
}

void RegisterController::start(const QString &parentUsername)
{
    m_parentUsername = parentUsername;
    m_registerWindow = new RegisterWindow();
    
    connect(m_registerWindow, &RegisterWindow::registerRequested,
            this, &RegisterController::handleRegisterRequested);

    m_registerWindow->setWindowFlag(Qt::Window);
    m_registerWindow->setAttribute(Qt::WA_DeleteOnClose);
    m_registerWindow->show();
}

void RegisterController::handleRegisterRequested(const QString &childUsername, const QString &password)
{
    if (childUsername.isEmpty() || password.isEmpty()) {
        m_registerWindow->showError("Vui lòng nhập đầy đủ thông tin!");
        return;
    }

    m_accountService->createSubAccount(childUsername, password);
}

void RegisterController::handleAccountCreated(bool success, const QString &message)
{
    if (!m_registerWindow) return;

    if (success) {
        m_registerWindow->showSuccess(message);
        emit accountCreatedSuccessfully();
    } else {
        m_registerWindow->showError(message);
    }
}
