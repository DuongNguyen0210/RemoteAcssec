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
    void loginResult(bool success, const QString &role, const QString &message,
                     const QString &username);

private:
    void onLoginReply(QNetworkReply *reply, const QString &username);

};

#endif
