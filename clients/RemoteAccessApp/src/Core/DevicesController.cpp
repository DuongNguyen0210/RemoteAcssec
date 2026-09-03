#include "DevicesController.h"
#include "../GUI/Pages/DevicesPage.h"
#include "Store/DeviceStore.h"

DevicesController::DevicesController(DeviceStore *store, QObject *parent)
    : QObject(parent)
    , m_view(new DevicesPage())
    , m_store(store)
{
    if (m_store) {
        connect(m_store, &DeviceStore::devicesUpdated,
                this, &DevicesController::onDevicesUpdated);
    }

    connect(m_view, &DevicesPage::connectRequested,
            this, &DevicesController::onConnectRequested);
    connect(m_view, &DevicesPage::refreshRequested,
            this, &DevicesController::refresh);

    // Initial render if store already has cached devices
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
    emit connectRequested(childUsername);
}
