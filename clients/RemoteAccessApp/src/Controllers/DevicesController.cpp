#include "DevicesController.h"
#include "GUI/Pages/DevicesPage.h"
#include "Domain/Store/DeviceStore.h"
#include "Network/Http/DeviceService.h"
#include "Network/Relay/AdminSessionController.h"

DevicesController::DevicesController(DeviceStore *store, DeviceService *service, QObject *parent)
    : QObject(parent)
    , m_view(new DevicesPage())
    , m_store(store)
    , m_deviceService(service)
    , m_sessionController(new AdminSessionController(this))
{
    if (m_store) {
        connect(m_store, &DeviceStore::devicesUpdated,
                this, &DevicesController::onDevicesUpdated);
    }

    connect(m_view, &DevicesPage::connectRequested,
            this, &DevicesController::onConnectRequested);
    connect(m_view, &DevicesPage::refreshRequested,
            this, &DevicesController::refresh);

    connect(m_deviceService, &DeviceService::devicesResult, this,
            [this](bool success, const QList<DeviceInfo>& devices, const QString& message) {
        if (success) m_store->replaceDevices(devices);
        else if (m_view) m_view->showError(message);
    });

    connect(m_sessionController, &AdminSessionController::sessionEstablished,
            this, &DevicesController::handleSessionEstablished);

    connect(m_sessionController, &AdminSessionController::requestFailed,
            this, [this](const QString &agentSessionId, const QString &reason)
            {
                Q_UNUSED(agentSessionId);
                handleSessionFailed(reason);
            });

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
        m_deviceService->fetchDevices();
    }
}

void DevicesController::onDevicesUpdated(const QList<DeviceInfo> &devices)
{
    if (m_view) {
        m_view->updateDeviceList(devices);
    }
}

void DevicesController::onConnectRequested(const QString &agentSessionId)
{
    m_sessionController->requestSession(agentSessionId);
}

void DevicesController::handleSessionEstablished(quint64 remoteSessionId, const QString &agentSessionId)
{
    emit remoteSessionStarted(remoteSessionId, agentSessionId);
}

void DevicesController::handleSessionFailed(const QString &reason)
{
    if (m_view) {
        m_view->showError(QStringLiteral("Không thể kết nối đến máy con: %1").arg(reason));
    }
}
