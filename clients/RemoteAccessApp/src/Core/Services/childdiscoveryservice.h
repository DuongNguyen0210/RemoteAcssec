#ifndef CHILDDISCOVERYSERVICE_H
#define CHILDDISCOVERYSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkReply;

class ChildDiscoveryService : public QObject
{
    Q_OBJECT

public:
    explicit ChildDiscoveryService(QObject *parent = nullptr);

    void loadChildren();

signals:
    void childrenLoaded(const QStringList &childUsernames);
    void loadFailed(const QString &message);

private:
    void handleReply(QNetworkReply *reply);
};

#endif // CHILDDISCOVERYSERVICE_H
