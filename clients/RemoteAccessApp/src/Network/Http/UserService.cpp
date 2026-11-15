#include "UserService.h"
#include "ApiClient.h"
#include <QJsonObject>
#include <QNetworkReply>

UserService::UserService(QObject *parent)
    : QObject(parent)
{
}

void UserService::fetchProfile()
{
    auto *reply = ApiClient::instance().get("/api/v1/user/profile");
    if (!reply) {
        emit profileResult(false, {}, {}, QStringLiteral("Không thể gửi yêu cầu lấy thông tin"));
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        auto response = ApiClient::parseReply(reply);
        reply->deleteLater();

        if (!response.success || !response.data.isObject()) {
            emit profileResult(false, {}, {}, response.message.isEmpty()
                    ? QStringLiteral("Không thể lấy thông tin tài khoản") : response.message);
            return;
        }

        auto obj = response.data.toObject();
        QString username = obj.value("username").toString();
        QString email = obj.value("email").toString();
        emit profileResult(true, username, email, QStringLiteral("Thành công"));
    });
}

void UserService::changePassword(const QString &oldPassword, const QString &newPassword)
{
    if (m_inFlight) return;

    QJsonObject body;
    body["oldPassword"] = oldPassword;
    body["newPassword"] = newPassword;

    auto *reply = ApiClient::instance().put("/api/v1/user/password", body);
    if (!reply) {
        emit changePasswordResult(false, QStringLiteral("Không thể gửi yêu cầu đổi mật khẩu"));
        return;
    }

    m_inFlight = true;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_inFlight = false;
        auto response = ApiClient::parseReply(reply);
        reply->deleteLater();

        emit changePasswordResult(response.success, response.message.isEmpty()
                ? (response.success ? QStringLiteral("Đổi mật khẩu thành công") : QStringLiteral("Đổi mật khẩu thất bại"))
                : response.message);
    });
}
