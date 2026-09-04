#ifndef ACCOUNTCONTROLLER_H
#define ACCOUNTCONTROLLER_H

#include <QObject>
#include <QString>
#include "Model/DeviceInfo.h"

class AccountPage;
class DeviceStore;

class AccountController : public QObject
{
    Q_OBJECT
public:
    explicit AccountController(DeviceStore *store, QObject *parent = nullptr);
    ~AccountController();

    AccountPage* getView() const;

public slots:
    void fetchAccounts();

private slots:
    void onDevicesUpdated(const QList<DeviceInfo> &devices);
    void onLoadFailed(const QString &errorMessage);
    void onAddAccountRequested();

signals:
    void requestAddAccount();

private:
    AccountPage *m_view;
    DeviceStore *m_store;
};

#endif // ACCOUNTCONTROLLER_H
