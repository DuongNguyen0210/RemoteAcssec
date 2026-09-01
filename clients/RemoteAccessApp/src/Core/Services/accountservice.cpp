#include "accountservice.h"
#include "Network/Client/apiclient.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>

AccountService::AccountService(QObject *parent) : QObject(parent)
{
}

void AccountService::createSubAccount(const QString &childUsername, const QString &password)
{
    QJsonObject json;
    json["childUsername"] = childUsername;
    json["password"] = password;

    QNetworkReply *reply = ApiClient::instance().post("/api/v1/child/Register", json);

    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onCreateAccountReply(reply);
    });
}

void AccountService::onCreateAccountReply(QNetworkReply *reply)
{
    reply->deleteLater();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Lỗi mạng hoặc không kết nối được server
    if (reply->error() != QNetworkReply::NoError && statusCode == 0) {
        emit createAccountResult(false, "Không thể kết nối tới Server");
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    
    bool success = false;
    QString message = "Lỗi không xác định";

    if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("success")) {
            success = obj["success"].toBool();
        }
        if (obj.contains("message")) {
            message = obj["message"].toString();
        }
    }

    if (statusCode == 200 && success) {
        emit createAccountResult(true, "Tạo tài khoản con thành công!");
    } else {
        emit createAccountResult(false, message);
    }
}

void AccountService::fetchListChildren()
{
    // Bạn có thể sửa URL này thành API Endpoint đúng của server
    QNetworkReply *reply = ApiClient::instance().get("/api/v1/child/list");
    
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onFetchListChildrenReply(reply);
    });
}

void AccountService::onFetchListChildrenReply(QNetworkReply *reply)
{
    reply->deleteLater();
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    if (reply->error() != QNetworkReply::NoError && statusCode == 0) {
        emit fetchListChildrenResult(false, QJsonArray(), "Không thể kết nối tới Server");
        return;
    }
    
    QByteArray responseData = reply->readAll();
    qDebug() << "HTTP Status:" << statusCode;
    qDebug() << "Raw Response:" << responseData;
    
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    
    // Giả sử API trả về class ListChillResponse: { "children": [...], "message": "..." }
    if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        QJsonArray children = obj["children"].toArray();
        QString message = obj["message"].toString();
        
        // Nếu HTTP Status là 200 thì coi như thành công
        if (statusCode == 200) {
            emit fetchListChildrenResult(true, children, message);
            return;
        } else {
            emit fetchListChildrenResult(false, QJsonArray(), message);
            return;
        }
    }
    
    emit fetchListChildrenResult(false, QJsonArray(), "Phản hồi từ server không hợp lệ. Chi tiết: " + QString(responseData));
}
