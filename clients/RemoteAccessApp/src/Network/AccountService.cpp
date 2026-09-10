#include "AccountService.h"
#include "ApiClient.h"
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
        success = obj.value("success").toBool(statusCode == 200);
        message = obj.value("message").toString("Lỗi không xác định");
    }

    if (statusCode == 200 && success) {
        emit createAccountResult(true, message.isEmpty() ? "Tạo tài khoản con thành công!" : message);
    } else {
        emit createAccountResult(false, message);
    }
}

void AccountService::fetchListChildren()
{
    // Hỗ trợ endpoint chuẩn /api/v1/child
    QNetworkReply *reply = ApiClient::instance().get("/api/v1/child");
    
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
    
    if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        bool success = obj.value("success").toBool(statusCode == 200);
        QString message = obj.value("message").toString();

        // Lấy danh sách máy con từ data của ApiResponse<List<ChildDto>>
        QJsonArray children;
        if (obj.value("data").isArray()) {
            children = obj.value("data").toArray();
        } else if (obj.value("children").isArray()) {
            children = obj.value("children").toArray();
        } else if (obj.value("child").isArray()) {
            children = obj.value("child").toArray();
        }
        
        if (statusCode == 200 && success) {
            emit fetchListChildrenResult(true, children, message);
            return;
        } else {
            emit fetchListChildrenResult(false, QJsonArray(), message.isEmpty() ? "Không thể lấy danh sách máy con" : message);
            return;
        }
    }
    
    emit fetchListChildrenResult(false, QJsonArray(), "Phản hồi từ server không hợp lệ. Chi tiết: " + QString(responseData));
}
