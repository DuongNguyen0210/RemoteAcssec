#include "AccountService.h"
#include "ApiClient.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QUrl>

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
        emit createAccountResult(false, QStringLiteral("Không gửi được yêu cầu. Thử lại."));
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
    if (m_fetching) return;
    QNetworkReply *reply = ApiClient::instance().get("/api/v1/child");
    if (!reply) {
        emit fetchListChildrenResult(false, QList<AccountInfo>(), QStringLiteral("Không gửi được yêu cầu. Thử lại."));
        return;
    }
    
    m_fetching = true;
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onFetchListChildrenReply(reply);
    });
}

void AccountService::onFetchListChildrenReply(QNetworkReply *reply)
{
    m_fetching = false;
    reply->deleteLater();
    
    ApiParsedResponse res = ApiClient::parseReply(reply);

    if (res.success) {
        if (!res.data.isArray()) {
            emit fetchListChildrenResult(false, {}, QStringLiteral("Dữ liệu tài khoản không hợp lệ"));
            return;
        }
        QList<AccountInfo> accounts;
        for (const auto &value : res.data.toArray()) {
            auto obj = value.toObject();
            AccountInfo account;
            account.id = obj.value("id").toVariant().toLongLong();
            account.username = obj.value("username").toString();
            account.childUsername = obj.value("childUsername").toString();
            accounts.append(account);
        }
        emit fetchListChildrenResult(true, accounts, res.message);
    } else {
        emit fetchListChildrenResult(false, QList<AccountInfo>(), res.message.isEmpty() ? QStringLiteral("Chưa tải được tài khoản") : res.message);
    }
}

void AccountService::deleteSubAccount(const QString &childUsername)
{
    QNetworkReply *reply = ApiClient::instance().deleteResource("/api/v1/child/" + QString::fromLatin1(QUrl::toPercentEncoding(childUsername)));
    if (!reply) {
        emit deleteAccountResult(false, childUsername, QStringLiteral("Không gửi được yêu cầu. Thử lại."));
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

void AccountService::updateSubAccount(qint64 id, const QString &childUsername, const QString &newPassword)
{
    QJsonObject data{{"childUsername", childUsername}};
    if (!newPassword.isEmpty()) data["newPassword"] = newPassword;
    auto *reply = ApiClient::instance().put(QString("/api/v1/child/%1").arg(id), data);
    if (!reply) {
        emit updateAccountResult(id, false, QStringLiteral("Không thể gửi yêu cầu cập nhật"));
        return;
    }
    connect(reply, &QNetworkReply::finished, this, [this, reply, id]() {
        const auto result = ApiClient::parseReply(reply);
        reply->deleteLater();
        emit updateAccountResult(id, result.success, result.message);
    });
}
