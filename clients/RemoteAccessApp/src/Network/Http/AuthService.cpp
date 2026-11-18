#include "AuthService.h"
#include "ApiClient.h"
#include <QJsonObject>

AuthService::AuthService(QObject *parent) : QObject(parent) {}

void AuthService::login(const QString &username, const QString &password)
{
    QJsonObject json;
    json["username"] = username;
    json["password"] = password;

    QNetworkReply *reply = ApiClient::instance().post("/api/v1/auth/login", json);
    if (!reply) {
        emit loginResult(false, QString(), QStringLiteral("Không gửi được yêu cầu. Thử lại."), username);
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

    if (!res.success) {
        emit loginResult(false, {}, res.message, username);
        return;
    }
    const auto data = res.data.toObject();
    const auto role = data.value("role").toString();
    const auto token = data.value("token").toString();
    if ((role != "ADMIN" && role != "CHILD") || token.isEmpty()) {
        emit loginResult(false, {}, QStringLiteral("Phản hồi đăng nhập không hợp lệ."), username);
        return;
    }
    ApiClient::instance().setToken(token);
    emit loginResult(true, role, res.message, username);
}
