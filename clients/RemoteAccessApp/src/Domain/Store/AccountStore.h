#pragma once
#include <QObject>
#include <QList>
#include "Domain/Model/AccountInfo.h"

class AccountStore : public QObject {
    Q_OBJECT
public:
    explicit AccountStore(QObject *parent = nullptr) : QObject(parent) {}
    const QList<AccountInfo>& getAccounts() const { return m_accounts; }
    void replaceAccounts(const QList<AccountInfo>& values) {
        m_accounts = values;
        emit accountsUpdated(m_accounts);
    }
signals:
    void accountsUpdated(const QList<AccountInfo>& values);
private:
    QList<AccountInfo> m_accounts;
};
