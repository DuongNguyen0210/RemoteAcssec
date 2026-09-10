#ifndef DEVICEINFO_H
#define DEVICEINFO_H

#include <QString>
#include <QJsonObject>

struct DeviceInfo
{
    QString username;
    QString childUsername;
    QString password;
    QString ipAddress;
    QString os;
    bool isOnline;

    DeviceInfo()
        : isOnline(false)
    {}

    static DeviceInfo fromJson(const QJsonObject &obj)
    {
        DeviceInfo info;
        info.username = obj.value(QStringLiteral("username")).toString();
        info.childUsername = obj.value(QStringLiteral("childUsername")).toString();
        if (info.childUsername.isEmpty()) {
            info.childUsername = info.username;
        }
        if (info.username.isEmpty()) {
            info.username = info.childUsername;
        }
        info.password = obj.value(QStringLiteral("password")).toString();
        info.isOnline = obj.value(QStringLiteral("online")).toBool(false);
        info.ipAddress = obj.value(QStringLiteral("ipAddress")).toString(QStringLiteral("Local Network"));
        info.os = obj.value(QStringLiteral("os")).toString(QStringLiteral("Windows / Linux"));
        return info;
    }
};

#endif // DEVICEINFO_H
