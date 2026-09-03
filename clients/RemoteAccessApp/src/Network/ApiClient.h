#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QString>

class ApiClient : public QObject
{
    Q_OBJECT
public:
    static ApiClient& instance();

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
