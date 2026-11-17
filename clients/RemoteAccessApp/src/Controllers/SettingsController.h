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
    void loadSettings();

public slots:
    void handleSaveServerConfig(const QString &apiUrl);
    void handleResetServerConfig();
    void handleChangePassword(const QString &oldPassword, const QString &newPassword, const QString &confirmPassword);
    void handleSaveStreamSettings(int qualityIndex, bool keepAspectRatio);

signals:
    void statusMessage(const QString &message, bool isError);

private slots:
    void onChangePasswordResult(bool success, const QString &message);
    void onProfileResult(bool success, const QString &username, const QString &email, const QString &message);

private:
    SettingsPage *m_view;
    UserService  *m_userService;
};
