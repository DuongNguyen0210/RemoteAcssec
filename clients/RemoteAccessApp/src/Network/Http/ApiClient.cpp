#include "ApiClient.h"
#include <QJsonDocument>
#include <QUrl>
#include <QNetworkRequest>
#include <QDebug>

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
        result.message = reply->errorString();
        result.errorCode = QStringLiteral("NETWORK_ERROR");
        return result;
    }

    QByteArray body = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(body);

    if (doc.isObject()) {
        QJsonObject root = doc.object();
        result.success = root.value(QStringLiteral("success")).toBool(result.httpStatusCode >= 200 && result.httpStatusCode < 300);
        result.message = root.value(QStringLiteral("message")).toString();
        result.errorCode = root.value(QStringLiteral("errorCode")).toString();
        result.data = root.value(QStringLiteral("data"));
    } else {
        result.success = (result.httpStatusCode >= 200 && result.httpStatusCode < 300);
        result.message = result.success ? QStringLiteral("OK") : QString::fromUtf8(body);
    }

    return result;
}

ApiClient::ApiClient(QObject *parent) 
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this))
{
    m_baseUrl = "http://cornball-ibuprofen-polygraph.ngrok-free.dev";
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
    
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("ngrok-skip-browser-warning", "true");
    
    if (!m_token.isEmpty()) {
        qDebug() << "Token Request: " + m_token << '\n';
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
