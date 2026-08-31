#include "childdiscoveryservice.h"

#include "apiclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

ChildDiscoveryService::ChildDiscoveryService(QObject *parent)
    : QObject(parent)
{
}

void ChildDiscoveryService::loadChildren()
{
    QNetworkReply *reply = ApiClient::instance().get("/api/v1/child");
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleReply(reply);
    });
}

void ChildDiscoveryService::handleReply(QNetworkReply *reply)
{
    reply->deleteLater();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || statusCode != 200) {
        emit loadFailed(QStringLiteral("Khong the tai danh sach CHILD (HTTP %1).")
                                .arg(statusCode));
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    if (!document.isArray()) {
        emit loadFailed(QStringLiteral("Phan hoi danh sach CHILD khong hop le."));
        return;
    }

    QStringList childUsernames;
    const QJsonArray children = document.array();
    for (const QJsonValue &value : children) {
        if (!value.isObject()) {
            emit loadFailed(QStringLiteral("Phan hoi danh sach CHILD khong hop le."));
            return;
        }

        const QString childUsername = value.toObject()
                .value(QStringLiteral("childUsername")).toString();
        if (childUsername.isEmpty()) {
            emit loadFailed(QStringLiteral("Phan hoi thieu childUsername."));
            return;
        }
        childUsernames.append(childUsername);
    }

    emit childrenLoaded(childUsernames);
}
