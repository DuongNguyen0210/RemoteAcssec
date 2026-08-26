#include "Authservice.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkAccessManager>

#include <QDebug>

Authservice::Authservice(QObject *parent) : QObject(parent) {
    this->networkManager = new QNetworkAccessManager(this);
}

void Authservice::login(const QString &username, const QString &password)
{
    QJsonObject json;
    json["username"] = username;
    json["password"] = password;
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    QUrl url(API_URL);
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->post(request, data);

    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onLoginReply(reply);
    });

}

void Authservice::onLoginReply(QNetworkReply *reply)
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
        emit loginResult(true, Role, Message);
    }
    else if(statusCode == 401 || statusCode == 400)
    {
        QString Message = doc.object()["message"].toString();
        qDebug() << "Network/Authservice.cpp: " + Message << '\n';
        emit loginResult(false, "", Message);
    }

}














