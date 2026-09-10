#ifndef DEVICESTORE_H
#define DEVICESTORE_H

#include <QObject>
#include <QList>
#include <QJsonArray>
#include "../Model/DeviceInfo.h"

class AccountService;

class DeviceStore : public QObject
{
    Q_OBJECT
public:
    explicit DeviceStore(QObject *parent = nullptr);
    ~DeviceStore();

    void refresh();
    QList<DeviceInfo> getDevices() const;
    DeviceInfo getDevice(const QString &childUsername) const;
    void updateDeviceStatus(const QString &childUsername, bool isOnline);

signals:
    void devicesUpdated(const QList<DeviceInfo> &devices);
    void loadFailed(const QString &errorMessage);

private slots:
    void handleFetchResult(bool success, const QJsonArray &children, const QString &message);

private:
    AccountService *m_service;
    QList<DeviceInfo> m_devices;
};

#endif // DEVICESTORE_H
