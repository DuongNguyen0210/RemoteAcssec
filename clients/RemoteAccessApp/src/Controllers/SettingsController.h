#pragma once

#include <QObject>
#include <QString>

class SettingsPage;
class UserService;

class SettingsController : public QObject
{
    Q_OBJECT
public:
    explicit SettingsController(SettingsPage *view, UserService *userService, QObject *parent = nullptr);

    SettingsPage* getView() const { return m_view; }
    void setKnownUsername(const QString &username);
    void loadSettings();

public slots:
    void handleChangePassword(const QString &oldPassword, const QString &newPassword, const QString &confirmPassword);

signals:
    void statusMessage(const QString &message, bool isError);

private slots:
    void onChangePasswordResult(bool success, const QString &message);
    void onProfileResult(bool success, const QString &username, const QString &email, const QString &message);

private:
    SettingsPage *m_view;
    UserService  *m_userService;
    QString m_knownUsername;
    bool m_passwordChangePending = false;
};
