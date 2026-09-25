#include "AccountCsvReader.h"
#include <QFile>
#include <QTextStream>
#include <QStringList>

static QString stripQuotes(QString str)
{
    str = str.trimmed();
    if ((str.startsWith('"') && str.endsWith('"')) ||
        (str.startsWith('\'') && str.endsWith('\''))) {
        if (str.length() >= 2) {
            str = str.mid(1, str.length() - 2).trimmed();
        }
    }
    return str;
}

QList<AccountInfo> AccountCsvReader::readAccounts(const QString &filePath, QString *errorMessage)
{
    QList<AccountInfo> accounts;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QString("Không thể mở file: %1").arg(file.errorString());
        }
        return accounts;
    }

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8")
#endif

    bool isFirstLine = true;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        int commaCount = line.count(',');
        int semiCount = line.count(';');
        int tabCount = line.count('\t');
        QChar separator = ',';
        if (semiCount > commaCount && semiCount > tabCount)
            separator = ';';
        else if (tabCount > commaCount && tabCount > semiCount)
            separator = '\t';


        QStringList parts = line.split(separator);
        if (parts.size() < 2) {
            continue;
        }

        QString col1 = stripQuotes(parts[0]);
        QString col2 = stripQuotes(parts[1]);

        if (isFirstLine) {
            isFirstLine = false;
            QString lowerCol1 = col1.toLower();
            QString lowerCol2 = col2.toLower();
            bool isHeader = (lowerCol1 == "childusername" || lowerCol1 == "username" ||
                             lowerCol1 == "user" || lowerCol1 == "tên tài khoản" ||
                             lowerCol1 == "tài khoản" || lowerCol1 == "account" ||
                             lowerCol2 == "password" || lowerCol2 == "pass" ||
                             lowerCol2 == "mật khẩu" || lowerCol2 == "matkhau");
            if (isHeader) {
                continue;
            }
        }

        if (!col1.isEmpty() && !col2.isEmpty()) {
            AccountInfo acc;
            acc.childUsername = col1;
            acc.username = col1;
            acc.password = col2;
            accounts.append(acc);
        }
    }

    file.close();

    if (accounts.isEmpty() && errorMessage && errorMessage->isEmpty()) {
        *errorMessage = QString("Không tìm thấy dữ liệu tài khoản hợp lệ trong file.");
    }

    return accounts;
}
