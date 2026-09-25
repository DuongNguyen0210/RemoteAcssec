#include "ApiClient.h"
#include <QJsonDocument>
#include <QUrl>
#include <QNetworkRequest>
#include <QDebug>
#include <QHash>

ApiClient& ApiClient::instance()
{
    static ApiClient _instance;
    return _instance;
}

ApiParsedResponse ApiClient::parseReply(QNetworkReply *reply)
{
    ApiParsedResponse result;
    result.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError && result.httpStatusCode == 0) {
        result.success = false;
        result.message = reply->error() == QNetworkReply::TimeoutError
            ? QStringLiteral("Kết nối quá hạn. Thử lại.")
            : QStringLiteral("Không kết nối được máy chủ. Kiểm tra mạng và thử lại.");
        result.errorCode = QStringLiteral("NETWORK_ERROR");
        return result;
    }

    QByteArray body = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(body);

    if (doc.isObject())
    {
        QJsonObject root = doc.object();
        result.success = root.value(QStringLiteral("success")).toBool(result.httpStatusCode >= 200 && result.httpStatusCode < 300);
        result.message = root.value(QStringLiteral("message")).toString();
        result.errorCode = root.value(QStringLiteral("errorCode")).toString();
        result.data = root.value(QStringLiteral("data"));
    }
    else
    {
        result.success = (result.httpStatusCode >= 200 && result.httpStatusCode < 300);
        result.message = result.success ? QStringLiteral("OK") : QString::fromUtf8(body);
    }

    if (result.success) {
        result.message = QStringLiteral("Đã thực hiện");
    } else {
        static const QHash<QString, QString> messages = {
            {"AUTH_FAILED", "Sai tên đăng nhập hoặc mật khẩu."},
            {"CHILD_ALREADY_EXISTS", "Tên tài khoản đã tồn tại."},
            {"USER_ALREADY_EXISTS", "Tên đăng nhập đã tồn tại."},
            {"CHILD_NOT_FOUND", "Tài khoản không còn tồn tại."},
            {"USER_NOT_FOUND", "Không tìm thấy tài khoản."},
            {"LIMIT_EXCEEDED", "Đã đạt giới hạn tài khoản."},
            {"FORBIDDEN", "Bạn không có quyền thực hiện."},
            {"UNAUTHORIZED", "Phiên đã hết hạn. Đăng nhập lại."},
            {"INVALID_INPUT", "Kiểm tra lại tên tài khoản và mật khẩu."},
            {"VALIDATION_ERROR", "Thông tin chưa hợp lệ. Kiểm tra lại."},
            {"INVALID_OLD_PASSWORD", "Mật khẩu cũ không đúng."},
            {"SERVICE_UNAVAILABLE", "Máy chủ tạm không hoạt động. Thử lại sau."}
        };
        QString fallback = QStringLiteral("Không thực hiện được. Thử lại sau.");
        if (result.httpStatusCode == 401) fallback = QStringLiteral("Phiên đã hết hạn. Đăng nhập lại.");
        if (result.httpStatusCode == 403) fallback = QStringLiteral("Bạn không có quyền thực hiện.");
        if (result.httpStatusCode == 409) fallback = QStringLiteral("Tên tài khoản đã tồn tại.");
        result.message = messages.value(result.errorCode, fallback);
    }
    return result;
}

ApiClient::ApiClient(QObject *parent) 
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this))
{
    m_baseUrl = qEnvironmentVariable("REMOTE_API_URL", "http://cornball-ibuprofen-polygraph.ngrok-free.dev");
}

ApiClient::~ApiClient()
{
}

void ApiClient::setToken(const QString &token)
{
    m_token = token;
}

QString ApiClient::getToken() const
{
    return m_token;
}

QNetworkRequest ApiClient::createRequest(const QString &endpoint) const
{
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest request(url);
    request.setTransferTimeout(10000);
    
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("ngrok-skip-browser-warning", "true");
    
    if (!m_token.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    }
    
    return request;
}

QNetworkReply* ApiClient::post(const QString &endpoint, const QJsonObject &data)
{
    QNetworkRequest request = createRequest(endpoint);
    QJsonDocument doc(data);
    return m_networkManager->post(request, doc.toJson());
}

QNetworkReply* ApiClient::get(const QString &endpoint)
{
    QNetworkRequest request = createRequest(endpoint);
    return m_networkManager->get(request);
}

QNetworkReply* ApiClient::deleteResource(const QString &endpoint)
{
    QNetworkRequest request = createRequest(endpoint);
    return m_networkManager->deleteResource(request);
}

QNetworkReply* ApiClient::put(const QString &endpoint, const QJsonObject &data)
{
    return m_networkManager->put(createRequest(endpoint), QJsonDocument(data).toJson());
}
