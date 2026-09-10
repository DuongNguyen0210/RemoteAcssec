#ifndef ACCOUNTSERVICE_H
#define ACCOUNTSERVICE_H

#include <QObject>
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

signals:
    void createAccountResult(bool success, const QString &message);
    void fetchListChildrenResult(bool success, const QJsonArray &children, const QString &message);

private slots:
    void onCreateAccountReply(QNetworkReply *reply);
    void onFetchListChildrenReply(QNetworkReply *reply);
};

#endif // ACCOUNTSERVICE_H
