#include "CreateAccountController.h"

#include "GUI/Dialogs/CreateAccountDialog.h"
#include "Network/Http/AccountService.h"

CreateAccountController::CreateAccountController(QObject *parent)
    : QObject(parent)
    , m_dialog(nullptr)
    , m_accountService(new AccountService(this))
{
    connect(m_accountService, &AccountService::createAccountResult,
            this, &CreateAccountController::handleAccountCreated);
}

CreateAccountController::~CreateAccountController()
{
    if (m_dialog) {
        m_dialog->deleteLater();
    }
}

void CreateAccountController::start(const QString &parentUsername)
{
    m_parentUsername = parentUsername;
    m_dialog = new CreateAccountDialog();

    connect(m_dialog, &CreateAccountDialog::registerRequested,
            this, &CreateAccountController::handleRegisterRequested);

    m_dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_dialog->show();
}

void CreateAccountController::handleRegisterRequested(const QString &childUsername, const QString &password)
{
    if (childUsername.isEmpty() || password.isEmpty()) {
        if (m_dialog) {
            m_dialog->showError("Vui lòng nhập đầy đủ thông tin!");
        }
        return;
    }

    m_accountService->createSubAccount(childUsername, password);
}

void CreateAccountController::handleAccountCreated(bool success, const QString &message)
{
    if (!m_dialog) return;

    if (success) {
        m_dialog->showSuccess(message);
        emit accountCreatedSuccessfully();
    } else {
        m_dialog->showError(message);
    }
}
