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

ApiClient::ApiClient(QObject *parent) 
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this))
{
    m_baseUrl = "http://localhost:9090";
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
    // Bỏ qua trang cảnh báo của ngrok khi gọi qua API
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
