#ifndef ACCOUNTSERVICE_H
#define ACCOUNTSERVICE_H

#include <QObject>
#include <QList>
#include "Domain/Model/AccountInfo.h"
#include <QString>
#include <QNetworkReply>
#include <QJsonArray>

class AccountService : public QObject
{
    Q_OBJECT
public:
    explicit AccountService(QObject *parent = nullptr);

    void createSubAccount(const QString &childUsername, const QString &password);
    void fetchListChildren();
    void deleteSubAccount(const QString &childUsername);

signals:
    void createAccountResult(bool success, const QString &message);
    void fetchListChildrenResult(bool success, const QList<AccountInfo> &accounts, const QString &message);
    void deleteAccountResult(bool success, const QString &childUsername, const QString &message);

private:
    bool m_fetching = false;

private slots:
    void onCreateAccountReply(QNetworkReply *reply);
    void onFetchListChildrenReply(QNetworkReply *reply);
    void onDeleteAccountReply(QNetworkReply *reply, const QString &childUsername);
};

#endif
