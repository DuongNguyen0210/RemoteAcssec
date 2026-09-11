#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

struct ApiParsedResponse
{
    bool success;
    int httpStatusCode;
    QString message;
    QString errorCode;
    QJsonValue data;
};

class ApiClient : public QObject
{
    Q_OBJECT
public:
    static ApiClient& instance();
    static ApiParsedResponse parseReply(QNetworkReply *reply);

    void setToken(const QString &token);
    QString getToken() const;

    QNetworkReply* post(const QString &endpoint, const QJsonObject &data);
    QNetworkReply* get(const QString &endpoint);

private:
    explicit ApiClient(QObject *parent = nullptr);
    ~ApiClient();

    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    QNetworkRequest createRequest(const QString &endpoint) const;

    QNetworkAccessManager *m_networkManager;
    QString m_baseUrl;
    QString m_token;
};

#endif
