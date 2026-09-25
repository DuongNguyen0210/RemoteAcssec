#pragma once

#include <QString>
#include <QList>
#include "Domain/Model/AccountInfo.h"

class AccountCsvReader {
public:
    AccountCsvReader() = delete;
    static QList<AccountInfo> readAccounts(const QString &filePath, QString *errorMessage = nullptr);
};
