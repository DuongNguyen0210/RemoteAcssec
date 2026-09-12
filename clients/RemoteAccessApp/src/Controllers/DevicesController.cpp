#include "DevicesController.h"
#include "GUI/Pages/DevicesPage.h"
#include "Domain/Store/DeviceStore.h"
#include "Network/Http/AccountService.h"
#include "Network/Relay/AdminSessionController.h"
#include "GUI/Dialogs/ConfirmDialog.h"

DevicesController::DevicesController(DeviceStore *store, QObject *parent)
    : QObject(parent)
    , m_view(new DevicesPage())
    , m_store(store)
    , m_accountService(new AccountService(this))
    , m_sessionController(new AdminSessionController(this))
{
    if (m_store) {
        connect(m_store, &DeviceStore::devicesUpdated,
                this, &DevicesController::onDevicesUpdated);
    }

    connect(m_view, &DevicesPage::connectRequested,
            this, &DevicesController::onConnectRequested);
    connect(m_view, &DevicesPage::removeDeviceRequested,
            this, &DevicesController::onRemoveDeviceRequested);
    connect(m_view, &DevicesPage::refreshRequested,
            this, &DevicesController::refresh);

    connect(m_accountService, &AccountService::deleteAccountResult,
            this, &DevicesController::handleDeviceRemoved);

    connect(m_sessionController, &AdminSessionController::sessionEstablished,
            this, &DevicesController::handleSessionEstablished);
    connect(m_sessionController, &AdminSessionController::sessionFailed,
            this, &DevicesController::handleSessionFailed);

    if (m_store && !m_store->getDevices().isEmpty()) {
        m_view->updateDeviceList(m_store->getDevices());
    }
}

DevicesController::~DevicesController()
{
    if (m_view && m_view->parent() == nullptr) {
        m_view->deleteLater();
    }
}

DevicesPage* DevicesController::getView() const
{
    return m_view;
}

void DevicesController::refresh()
{
    if (m_store) {
        m_store->refresh();
    }
}

void DevicesController::onDevicesUpdated(const QList<DeviceInfo> &devices)
{
    if (m_view) {
        m_view->updateDeviceList(devices);
    }
}

void DevicesController::onConnectRequested(const QString &childUsername)
{
    m_connectingChildUsername = childUsername;
    m_sessionController->requestSession(childUsername);
}

void DevicesController::handleSessionEstablished(quint64 sessionId)
{
    emit remoteSessionStarted(sessionId, m_connectingChildUsername);
}

void DevicesController::handleSessionFailed(const QString &reason)
{
    if (m_view) {
        m_view->showError(QStringLiteral("Không thể kết nối đến máy con: %1").arg(reason));
    }
}

void DevicesController::onRemoveDeviceRequested(const QString &childUsername)
{
    if (!ConfirmDialog::confirmDelete(
            m_view,
            QStringLiteral("Xác nhận gỡ thiết bị"),
            QStringLiteral("Bạn có chắc chắn muốn gỡ thiết bị \"%1\" khỏi hệ thống không? Hành động này không thể hoàn tác.").arg(childUsername),
            QStringLiteral("Gỡ bỏ"),
            QStringLiteral("Hủy"))) {
        return;
    }

    m_accountService->deleteSubAccount(childUsername);
}

void DevicesController::handleDeviceRemoved(bool success, const QString &childUsername, const QString &message)
{
    Q_UNUSED(childUsername);
    if (success) {
        if (m_view) {
            m_view->showSuccess(message);
        }
        if (m_store) {
            m_store->refresh();
        }
    } else {
        if (m_view) {
            m_view->showError(message);
        }
    }
}
