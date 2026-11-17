#pragma once

#include <QObject>
#include <QString>

class UserService : public QObject
{
    Q_OBJECT
public:
    explicit UserService(QObject *parent = nullptr);

    void fetchProfile();
    void changePassword(const QString &oldPassword, const QString &newPassword);

signals:
    void profileResult(bool success, const QString &username, const QString &email, const QString &message);
    void changePasswordResult(bool success, const QString &message);

private:
    bool m_inFlight = false;
};
