#include "AuthService.h"
#include "ApiClient.h"
#include <QJsonObject>
#include <QJsonDocument>

#include <QDebug>

AuthService::AuthService(QObject *parent) : QObject(parent) {}

void AuthService::login(const QString &username, const QString &password)
{
    QJsonObject json;
    json["username"] = username;
    json["password"] = password;

    QNetworkReply *reply = ApiClient::instance().post("/api/v1/auth/login", json);
    if (!reply) {
        emit loginResult(false, QString(), QStringLiteral("Loi khoi tao yeu cau mang"), username);
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, username](){
        onLoginReply(reply, username);
    });
}

void AuthService::onLoginReply(QNetworkReply *reply, const QString &username)
{
    reply->deleteLater();

    ApiParsedResponse res = ApiClient::parseReply(reply);

    if (res.success) {
        QJsonObject dataObj = res.data.toObject();
        QString role = dataObj.contains("role") ? dataObj["role"].toString() : QString();
        QString token = dataObj.contains("token") ? dataObj["token"].toString() : QString();

        if (!token.isEmpty()) {
            ApiClient::instance().setToken(token);
        }

        emit loginResult(true, role, res.message, username);
    } else {
        QString msg = res.message.isEmpty() ? QStringLiteral("Authentication Failed") : res.message;
        emit loginResult(false, "", msg, username);
    }
}














