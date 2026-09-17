#include "AccountController.h"
#include "GUI/Components/Accounts/EditAccountDialog.h"
#include "GUI/Pages/AccountPage.h"
#include "GUI/Components/Accounts/CreateAccountDialog.h"
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

    connect(m_view, &AccountPage::editAccountRequested,
            this, &AccountController::onEditAccountRequested);
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
    connect(m_accountService, &AccountService::updateAccountResult, this,
            [this](qint64, bool success, const QString &) { if (success) fetchAccounts(); });
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
        m_view->showLoadError(message);
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
            m_createAccountDialog->accept();
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
            QStringLiteral("Xóa tài khoản \"%1\"? Không thể hoàn tác.").arg(username))) {
        return;
    }

    m_view->setActionsEnabled(false);
    m_accountService->deleteSubAccount(username);
}

void AccountController::handleAccountDeleted(bool success, const QString &childUsername, const QString &message)
{
    Q_UNUSED(childUsername);
    m_view->setActionsEnabled(true);
    if (success) {
        if (m_store) {
            fetchAccounts();
        }
    } else {
        if (m_view) {
            m_view->showError(message);
        }
    }
}

void AccountController::onEditAccountRequested(const QString &username)
{
    if (m_editAccountDialog) {
        m_editAccountDialog->raise();
        m_editAccountDialog->activateWindow();
        return;
    }
    for (const auto &account : m_store->getAccounts()) {
        if (account.username != username) continue;
        auto *dialog = new EditAccountDialog(account, m_view);
        m_editAccountDialog = dialog;
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &EditAccountDialog::saveRequested,
                m_accountService, &AccountService::updateSubAccount);
        connect(m_accountService, &AccountService::updateAccountResult, dialog,
                [this, dialog, id = account.id](qint64 updatedId, bool success, const QString &message) {
            if (updatedId != id) return;
            if (success) dialog->accept();
            else dialog->showError(message);
        });
        dialog->show();
        return;
    }
}
