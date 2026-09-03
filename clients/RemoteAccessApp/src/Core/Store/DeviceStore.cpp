#include "DeviceStore.h"
#include "../../Network/AccountService.h"
#include <QDebug>

DeviceStore::DeviceStore(QObject *parent)
    : QObject(parent)
    , m_service(new AccountService(this))
{
    connect(m_service, &AccountService::fetchListChildrenResult,
            this, &DeviceStore::handleFetchResult);
}

DeviceStore::~DeviceStore()
{
}

void DeviceStore::refresh()
{
    m_service->fetchListChildren();
}

QList<DeviceInfo> DeviceStore::getDevices() const
{
    return m_devices;
}

DeviceInfo DeviceStore::getDevice(const QString &childUsername) const
{
    for (const DeviceInfo &d : m_devices) {
        if (d.childUsername == childUsername || d.username == childUsername) {
            return d;
        }
    }
    return DeviceInfo();
}

void DeviceStore::updateDeviceStatus(const QString &childUsername, bool isOnline)
{
    bool changed = false;
    for (DeviceInfo &d : m_devices) {
        if (d.childUsername == childUsername || d.username == childUsername) {
            if (d.isOnline != isOnline) {
                d.isOnline = isOnline;
                changed = true;
            }
            break;
        }
    }

    if (changed) {
        emit devicesUpdated(m_devices);
    }
}

void DeviceStore::handleFetchResult(bool success, const QJsonArray &children, const QString &message)
{
    if (!success) {
        qWarning() << "[DeviceStore] Fetch devices failed:" << message;
        emit loadFailed(message);
        return;
    }

    m_devices.clear();
    for (const QJsonValue &val : children) {
        if (val.isObject()) {
            m_devices.append(DeviceInfo::fromJson(val.toObject()));
        }
    }

    qDebug() << "[DeviceStore] Cached" << m_devices.size() << "devices in memory.";
    emit devicesUpdated(m_devices);
}
