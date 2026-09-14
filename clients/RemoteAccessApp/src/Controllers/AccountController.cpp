#include "AccountController.h"
#include "GUI/Pages/AccountPage.h"
#include "GUI/Dialogs/CreateAccountDialog.h"
#include "Domain/Store/AccountStore.h"
#include "Network/Http/AccountService.h"
#include "GUI/Dialogs/ConfirmDialog.h"

AccountController::AccountController(AccountStore *store, AccountService *service, QObject *parent)
    : QObject(parent)
    , m_view(new AccountPage())
    , m_store(store)
    , m_accountService(service)
    , m_createAccountDialog(nullptr)
{
    if (m_store) {
        connect(m_store, &AccountStore::accountsUpdated,
                this, &AccountController::onAccountsUpdated);

        if (!m_store->getAccounts().isEmpty()) {
            m_view->updateAccountList(m_store->getAccounts());
        }
    }

    connect(m_view, &AccountPage::deleteAccountRequested,
            this, &AccountController::onDeleteAccountRequested);
    connect(m_view, &AccountPage::requestAddAccount,
            this, &AccountController::onAddAccountRequested);
    connect(m_view, &AccountPage::loadRequested,
            this, &AccountController::fetchAccounts);

    connect(m_accountService, &AccountService::fetchListChildrenResult, this,
            [this](bool success, const QList<AccountInfo>& accounts, const QString& message) {
        if (success) m_store->replaceAccounts(accounts);
        else onLoadFailed(message);
    });
    connect(m_accountService, &AccountService::createAccountResult,
            this, &AccountController::handleAccountCreated);
    connect(m_accountService, &AccountService::deleteAccountResult,
            this, &AccountController::handleAccountDeleted);
}

AccountController::~AccountController()
{
    if (m_createAccountDialog) {
        m_createAccountDialog->deleteLater();
    }
    if (m_view && m_view->parent() == nullptr) {
        m_view->deleteLater();
    }
}

AccountPage* AccountController::getView() const
{
    return m_view;
}

void AccountController::fetchAccounts()
{
    m_view->showLoading();
    m_accountService->fetchListChildren();
}

void AccountController::onAccountsUpdated(const QList<AccountInfo> &accounts)
{
    if (m_view) {
        m_view->updateAccountList(accounts);
    }
}

void AccountController::onLoadFailed(const QString &message)
{
    if (m_view) {
        m_view->updateAccountList(m_store->getAccounts());
        m_view->showError(message);
    }
}

void AccountController::onAddAccountRequested()
{
    if (m_createAccountDialog) {
        m_createAccountDialog->raise();
        m_createAccountDialog->activateWindow();
        return;
    }

    m_createAccountDialog = new CreateAccountDialog(m_view);
    m_createAccountDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_createAccountDialog, &CreateAccountDialog::registerRequested,
            this, &AccountController::handleRegisterRequested);
    m_createAccountDialog->show();
}

void AccountController::handleRegisterRequested(const QString &childUsername, const QString &password)
{
    if (childUsername.isEmpty() || password.isEmpty()) {
        if (m_createAccountDialog) {
            m_createAccountDialog->showError(QStringLiteral("Vui lòng nhập đầy đủ thông tin!"));
        }
        return;
    }

    m_accountService->createSubAccount(childUsername, password);
}

void AccountController::handleAccountCreated(bool success, const QString &message)
{
    if (m_createAccountDialog) {
        if (success) {
            m_createAccountDialog->showSuccess(message);
        } else {
            m_createAccountDialog->showError(message);
        }
    }

    if (success && m_store) {
        fetchAccounts();
    }
}

void AccountController::onDeleteAccountRequested(const QString &username)
{
    if (!ConfirmDialog::confirmDelete(
            m_view,
            QStringLiteral("Xác nhận xóa tài khoản"),
            QStringLiteral("Bạn có chắc chắn muốn xóa tài khoản con \"%1\" không? Hành động này không thể hoàn tác.").arg(username))) {
        return;
    }

    m_accountService->deleteSubAccount(username);
}

void AccountController::handleAccountDeleted(bool success, const QString &childUsername, const QString &message)
{
    Q_UNUSED(childUsername);
    if (success) {
        ConfirmDialog::showInfo(m_view, QStringLiteral("Thành công"), message);
        if (m_store) {
            fetchAccounts();
        }
    } else {
        if (m_view) {
            m_view->showError(message);
        }
    }
}
