#include "SettingsController.h"
#include "GUI/Pages/SettingsPage.h"
#include "Network/Http/UserService.h"
#include "Network/Http/ApiClient.h"


SettingsController::SettingsController(SettingsPage *view, UserService *userService, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_userService(userService)
{
    if (m_userService) {
        connect(m_userService, &UserService::changePasswordResult,
                this, &SettingsController::onChangePasswordResult);
        connect(m_userService, &UserService::profileResult,
                this, &SettingsController::onProfileResult);
    }

    if (m_view) {
        connect(m_view, &SettingsPage::changePasswordRequested,
                this, &SettingsController::handleChangePassword);
    }
}

void SettingsController::loadSettings()
{
    if (m_userService) {
        m_userService->fetchProfile();
    }
}

void SettingsController::handleChangePassword(const QString &oldPassword, const QString &newPassword, const QString &confirmPassword)
{
    if (m_passwordChangePending) return;
    if (oldPassword.isEmpty()) {
        if (m_view) m_view->showFeedback(QStringLiteral("Vui lòng nhập mật khẩu hiện tại."), true);
        return;
    }

    if (newPassword.trimmed().isEmpty() || newPassword.length() < 6) {
        if (m_view) m_view->showFeedback(QStringLiteral("Mật khẩu mới phải có ít nhất 6 ký tự và không chỉ chứa khoảng trắng."), true);
        return;
    }

    if (newPassword.length() > 255) {
        if (m_view) m_view->showFeedback(QStringLiteral("Mật khẩu mới tối đa 255 ký tự."), true);
        return;
    }

    if (newPassword != confirmPassword) {
        if (m_view) m_view->showFeedback(QStringLiteral("Mật khẩu xác nhận không khớp."), true);
        return;
    }

    if (oldPassword == newPassword) {
        if (m_view) m_view->showFeedback(QStringLiteral("Mật khẩu mới không được trùng mật khẩu cũ."), true);
        return;
    }

    if (m_userService) {
        m_passwordChangePending = true;
        if (m_view) m_view->setPasswordChangePending(true);
        m_userService->changePassword(oldPassword, newPassword);
    }
}

void SettingsController::onChangePasswordResult(bool success, const QString &message)
{
    m_passwordChangePending = false;
    if (m_view) m_view->setPasswordChangePending(false);
    if (m_view) {
        m_view->showFeedback(message, !success);
        if (success) {
            m_view->clearPasswordInputs();
        }
    }
}

void SettingsController::setKnownUsername(const QString &username)
{
    m_knownUsername = username.trimmed();
    if (m_view && !m_knownUsername.isEmpty()) {
        m_view->setProfileInfo(m_knownUsername, QString());
    }
}

void SettingsController::onProfileResult(bool success, const QString &username, const QString &email, const QString &message)
{
    Q_UNUSED(message);
    if (!m_view) return;
    if (success) {
        const QString resolvedUsername = username.isEmpty() ? m_knownUsername : username;
        if (!resolvedUsername.isEmpty()) m_knownUsername = resolvedUsername;
        m_view->setProfileInfo(
            resolvedUsername.isEmpty() ? QStringLiteral("Không xác định") : resolvedUsername,
            email.isEmpty() ? QStringLiteral("Chưa thiết lập") : email
        );
    } else if (!m_knownUsername.isEmpty()) {
        m_view->setProfileInfo(m_knownUsername, QString());
    } else {
        m_view->setProfileInfo(QStringLiteral("Không thể tải"), QString());
    }
}
