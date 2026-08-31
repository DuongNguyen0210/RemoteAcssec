#include "accountcontroller.h"
#include "../GUI/Pages/accountpage.h"
#include "../Network/accountservice.h"

AccountController::AccountController(QObject *parent)
    : QObject(parent),
      m_view(new AccountPage()),
      m_service(new AccountService(this))
{
    // Cấp cho View quyền gọi Controller
    m_view->setController(this);

    connect(m_service, &AccountService::fetchListChildrenResult,
            this, &AccountController::onFetchResult);
            
    connect(m_view, &AccountPage::requestAddAccount,
            this, &AccountController::onAddAccountRequested);
}

AccountController::~AccountController()
{
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
    m_service->fetchListChildren();
}

void AccountController::onFetchResult(bool success, const QJsonArray &children, const QString &message)
{
    if (success) {
        m_view->updateAccountList(children);
    } else {
        m_view->showError(message);
    }
}

void AccountController::onAddAccountRequested()
{
    emit requestAddAccount();
}
