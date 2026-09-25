#ifndef ACCOUNTCONTROLLER_H
#define ACCOUNTCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QString>
#include "Domain/Model/AccountInfo.h"

class AccountPage;
class AccountStore;
class AccountService;
class CreateAccountDialog;
class EditAccountDialog;
class AccountCsvReader;

class AccountController : public QObject
{
    Q_OBJECT
public:
    explicit AccountController(AccountStore *store, AccountService *service, QObject *parent = nullptr);
    ~AccountController();

    AccountPage* getView() const;

public slots:
    void fetchAccounts();

private slots:
    void onAccountsUpdated(const QList<AccountInfo> &devices);
    void onLoadFailed(const QString &errorMessage);
    void onAddAccountRequested();
    void onEditAccountRequested(const QString &username);
    void onDeleteAccountRequested(const QString &username);
    void handleRegisterRequested(const QString &childUsername, const QString &password, const QString &filepath);
    void handleAccountCreated(bool success, const QString &message);
    void handleAccountDeleted(bool success, const QString &childUsername, const QString &message);

private:
    QPointer<AccountPage> m_view;
    AccountStore *m_store;
    AccountService *m_accountService;
    QPointer<CreateAccountDialog> m_createAccountDialog;
    QPointer<EditAccountDialog> m_editAccountDialog;

    int m_pendingImportCount = 0;
    int m_successImportCount = 0;
    int m_failedImportCount = 0;
    QStringList m_importErrors;
};

#endif
