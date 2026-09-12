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
    if (!reply) {
        emit createAccountResult(false, QStringLiteral("Loi khoi tao yeu cau mang"));
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onCreateAccountReply(reply);
    });
}

void AccountService::onCreateAccountReply(QNetworkReply *reply)
{
    reply->deleteLater();

    ApiParsedResponse res = ApiClient::parseReply(reply);

    if (res.success) {
        emit createAccountResult(true, res.message.isEmpty() ? QStringLiteral("Tạo tài khoản con thành công!") : res.message);
    } else {
        emit createAccountResult(false, res.message.isEmpty() ? QStringLiteral("Lỗi không xác định") : res.message);
    }
}

void AccountService::fetchListChildren()
{
    QNetworkReply *reply = ApiClient::instance().get("/api/v1/child");
    if (!reply) {
        emit fetchListChildrenResult(false, QJsonArray(), QStringLiteral("Loi khoi tao yeu cau mang"));
        return;
    }
    
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onFetchListChildrenReply(reply);
    });
}

void AccountService::onFetchListChildrenReply(QNetworkReply *reply)
{
    reply->deleteLater();
    
    ApiParsedResponse res = ApiClient::parseReply(reply);

    if (res.success) {
        emit fetchListChildrenResult(true, res.data.toArray(), res.message);
    } else {
        emit fetchListChildrenResult(false, QJsonArray(), res.message.isEmpty() ? QStringLiteral("Không thể lấy danh sách máy con") : res.message);
    }
}

void AccountService::deleteSubAccount(const QString &childUsername)
{
    QNetworkReply *reply = ApiClient::instance().deleteResource("/api/v1/child/" + childUsername);
    if (!reply) {
        emit deleteAccountResult(false, childUsername, QStringLiteral("Loi khoi tao yeu cau mang"));
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, childUsername](){
        onDeleteAccountReply(reply, childUsername);
    });
}

void AccountService::onDeleteAccountReply(QNetworkReply *reply, const QString &childUsername)
{
    reply->deleteLater();

    ApiParsedResponse res = ApiClient::parseReply(reply);

    if (res.success) {
        emit deleteAccountResult(true, childUsername, res.message.isEmpty() ? QStringLiteral("Xóa tài khoản thành công!") : res.message);
    } else {
        emit deleteAccountResult(false, childUsername, res.message.isEmpty() ? QStringLiteral("Không thể xóa tài khoản con") : res.message);
    }
}
