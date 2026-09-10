#ifndef DEVICESCONTROLLER_H
#define DEVICESCONTROLLER_H

#include <QObject>
#include <QString>
#include "Model/DeviceInfo.h"

class DevicesPage;
class DeviceStore;

class DevicesController : public QObject
{
    Q_OBJECT
public:
    explicit DevicesController(DeviceStore *store, QObject *parent = nullptr);
    ~DevicesController();

    DevicesPage* getView() const;
    void refresh();

signals:
    void connectRequested(const QString &childUsername);

private slots:
    void onDevicesUpdated(const QList<DeviceInfo> &devices);
    void onConnectRequested(const QString &childUsername);

private:
    DevicesPage *m_view;
    DeviceStore *m_store;
};

#endif // DEVICESCONTROLLER_H
