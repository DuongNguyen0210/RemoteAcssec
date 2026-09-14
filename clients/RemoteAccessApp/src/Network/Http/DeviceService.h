#pragma once
#include <QObject>
#include <QList>
#include "Domain/Model/DeviceInfo.h"

class DeviceService : public QObject {
    Q_OBJECT
public:
    explicit DeviceService(QObject *parent = nullptr) : QObject(parent) {}
    void fetchDevices();
signals:
    void devicesResult(bool success, const QList<DeviceInfo>& devices, const QString& message);
private:
    bool m_loading = false;
};
