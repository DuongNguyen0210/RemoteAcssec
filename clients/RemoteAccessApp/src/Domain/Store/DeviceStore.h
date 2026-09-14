#pragma once
#include <QObject>
#include <QList>
#include "Domain/Model/DeviceInfo.h"

class DeviceStore : public QObject {
    Q_OBJECT
public:
    explicit DeviceStore(QObject *parent = nullptr) : QObject(parent) {}
    const QList<DeviceInfo>& getDevices() const { return m_devices; }
    void replaceDevices(const QList<DeviceInfo>& values) {
        m_devices = values;
        emit devicesUpdated(m_devices);
    }
signals:
    void devicesUpdated(const QList<DeviceInfo>& values);
private:
    QList<DeviceInfo> m_devices;
};
