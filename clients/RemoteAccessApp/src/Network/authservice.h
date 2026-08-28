#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>
#include <QString>
#include <QNetworkReply>

class AuthService : public QObject
{
    Q_OBJECT
public:
    explicit AuthService(QObject *parent = nullptr);
    void login(const QString &username, const QString &password);

signals:
    void loginResult(bool Success, const QString &Role, const QString Message);

private slots:
    void onLoginReply(QNetworkReply *reply);

private:
};

#endif
