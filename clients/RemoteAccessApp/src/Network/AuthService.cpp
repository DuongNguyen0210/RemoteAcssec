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

    connect(reply, &QNetworkReply::finished, this, [this, reply, username](){
        onLoginReply(reply, username);
    });
}

void AuthService::onLoginReply(QNetworkReply *reply, const QString &username)
{
    reply->deleteLater();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if(reply->error() != QNetworkReply::NoError && statusCode == 0)
    {
        emit loginResult(false, "", "Connect Error", username);
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    QJsonObject root = doc.object();

    bool success = root.value("success").toBool(statusCode == 200);
    QString message = root.value("message").toString();

    if(statusCode == 200 && success)
    {
        QJsonObject dataObj = root.value("data").toObject();
        // Hỗ trợ cả ApiResponse chuẩn (data.role) và fallback root-level (role)
        QString role = dataObj.contains("role") ? dataObj["role"].toString() : root["role"].toString();
        QString token = dataObj.contains("token") ? dataObj["token"].toString() : root["token"].toString();
        
        if (!token.isEmpty()) {
            qDebug() << "Token: " + token << '\n';
            ApiClient::instance().setToken(token);
        }

        emit loginResult(true, role, message, username);
    }
    else
    {
        if (message.isEmpty()) {
            message = "Authentication Failed";
        }
        emit loginResult(false, "", message, username);
    }
}














