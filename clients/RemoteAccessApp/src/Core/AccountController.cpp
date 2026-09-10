#include "AccountController.h"
#include "../GUI/Pages/AccountPage.h"
#include "Store/DeviceStore.h"

AccountController::AccountController(DeviceStore *store, QObject *parent)
    : QObject(parent),
      m_view(new AccountPage()),
      m_store(store)
{
    m_view->setController(this);

    if (m_store) {
        connect(m_store, &DeviceStore::devicesUpdated,
                this, &AccountController::onDevicesUpdated);
        connect(m_store, &DeviceStore::loadFailed,
                this, &AccountController::onLoadFailed);
        
        if (!m_store->getDevices().isEmpty()) {
            m_view->updateAccountList(m_store->getDevices());
        }
    }
            
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
    if (m_store) {
        m_store->refresh();
    }
}

void AccountController::onDevicesUpdated(const QList<DeviceInfo> &devices)
{
    if (m_view) {
        m_view->updateAccountList(devices);
    }
}

void AccountController::onLoadFailed(const QString &message)
{
    if (m_view) {
        m_view->showError(message);
    }
}

void AccountController::onAddAccountRequested()
{
    emit requestAddAccount();
}
