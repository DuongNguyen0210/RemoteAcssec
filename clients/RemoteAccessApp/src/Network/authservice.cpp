#include "authservice.h"
#include "apiclient.h"
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

    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onLoginReply(reply);
    });
}

void AuthService::onLoginReply(QNetworkReply *reply)
{
    reply->deleteLater();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if(reply->error() != QNetworkReply::NoError && statusCode == 0)
    {
        emit loginResult(false, "", "Connect Error");
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if(statusCode == 200)
    {
        QString Role = doc.object()["role"].toString();
        QString Message = doc.object()["message"].toString();
        
        if (doc.object().contains("token")) {
            QString token = doc.object()["token"].toString();
            qDebug() << "Token: " + token << '\n';
            ApiClient::instance().setToken(token);
        }

        emit loginResult(true, Role, Message);
    }
    else if(statusCode == 401 || statusCode == 400)
    {
        QString Message = doc.object()["message"].toString();
        emit loginResult(false, "", Message);
    }

}














