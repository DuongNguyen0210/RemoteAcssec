#ifndef ACCOUNTCONTROLLER_H
#define ACCOUNTCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QString>
#include "Domain/Model/DeviceInfo.h"

class AccountPage;
class DeviceStore;
class AccountService;
class CreateAccountDialog;

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
    void onDeleteAccountRequested(const QString &username);
    void handleRegisterRequested(const QString &childUsername, const QString &password);
    void handleAccountCreated(bool success, const QString &message);
    void handleAccountDeleted(bool success, const QString &childUsername, const QString &message);

private:
    AccountPage *m_view;
    DeviceStore *m_store;
    AccountService *m_accountService;
    QPointer<CreateAccountDialog> m_createAccountDialog;
};

#endif
