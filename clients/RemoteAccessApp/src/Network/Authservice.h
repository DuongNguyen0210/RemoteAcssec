#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class Authservice : public QObject
{
    Q_OBJECT
public:
    explicit Authservice(QObject *parent = nullptr);
    void login(const QString &username, const QString &password);

signals:
    void loginResult(bool Success, const QString &Role, const QString Message);

private slots:
    void onLoginReply(QNetworkReply *reply);

private:
    QNetworkAccessManager * networkManager;
    const QString API_URL = "http://cornball-ibuprofen-polygraph.ngrok-free.dev/api/v1/auth/login";
};

#endif
