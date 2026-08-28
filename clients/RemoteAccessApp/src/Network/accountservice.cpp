#include "accountservice.h"
#include "apiclient.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>

AccountService::AccountService(QObject *parent) : QObject(parent)
{
}

void AccountService::createSubAccount(const QString &childUsername, const QString &password, const QString &parentUsername)
{
    QJsonObject json;
    json["childUsername"] = childUsername;
    json["password"] = password;
    
    // Vì Backend hiện tại RegisterRequest yêu cầu một object User, ta truyền vào JSON tương ứng
    QJsonObject userObj;
    userObj["username"] = parentUsername;
    json["user"] = userObj;

    // Gửi yêu cầu POST xuống API Register của bảng Child
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
