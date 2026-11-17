#include "SettingsController.h"
#include "GUI/Pages/SettingsPage.h"
#include "Network/Http/UserService.h"
#include "Network/Http/ApiClient.h"

#include <QSettings>
#include <QUrl>
#include <QDebug>

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
        connect(m_view, &SettingsPage::saveServerRequested,
                this, &SettingsController::handleSaveServerConfig);
        connect(m_view, &SettingsPage::resetServerRequested,
                this, &SettingsController::handleResetServerConfig);
        connect(m_view, &SettingsPage::changePasswordRequested,
                this, &SettingsController::handleChangePassword);
        connect(m_view, &SettingsPage::saveStreamSettingsRequested,
                this, &SettingsController::handleSaveStreamSettings);
    }
}

void SettingsController::loadSettings()
{
    QSettings settings("DUT_PBL4", "RemoteAccessApp");

    QString apiUrl = settings.value("server/apiUrl", ApiClient::instance().getBaseUrl()).toString();
    int qualityIndex = settings.value("streaming/qualityIndex", 1).toInt(); // 1 = Cân bằng (70%)
    bool keepAspect = settings.value("display/keepAspectRatio", true).toBool();

    if (m_view) {
        m_view->setServerUrl(apiUrl);
        m_view->setStreamSettings(qualityIndex, keepAspect);
    }

    if (m_userService) {
        m_userService->fetchProfile();
    }
}

void SettingsController::handleSaveServerConfig(const QString &apiUrl)
{
    QString trimmed = apiUrl.trimmed();
    QUrl url(trimmed);
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        if (m_view) {
            m_view->showFeedback(QStringLiteral("URL máy chủ không hợp lệ (phải bắt đầu bằng http:// hoặc https://)"), true);
        }
        return;
    }

    QSettings settings("DUT_PBL4", "RemoteAccessApp");
    settings.setValue("server/apiUrl", trimmed);

    ApiClient::instance().setBaseUrl(trimmed);

    if (m_view) {
        m_view->showFeedback(QStringLiteral("Đã lưu địa chỉ máy chủ thành công."), false);
    }
}

void SettingsController::handleResetServerConfig()
{
    QSettings settings("DUT_PBL4", "RemoteAccessApp");
    settings.remove("server/apiUrl");

    QString defaultUrl = qEnvironmentVariable("REMOTE_API_URL", "http://cornball-ibuprofen-polygraph.ngrok-free.dev");
    ApiClient::instance().setBaseUrl(defaultUrl);

    if (m_view) {
        m_view->setServerUrl(defaultUrl);
        m_view->showFeedback(QStringLiteral("Đã khôi phục địa chỉ máy chủ về mặc định."), false);
    }
}

void SettingsController::handleChangePassword(const QString &oldPassword, const QString &newPassword, const QString &confirmPassword)
{
    if (oldPassword.isEmpty()) {
        if (m_view) m_view->showFeedback(QStringLiteral("Vui lòng nhập mật khẩu hiện tại."), true);
        return;
    }

    if (newPassword.length() < 6) {
        if (m_view) m_view->showFeedback(QStringLiteral("Mật khẩu mới phải có ít nhất 6 ký tự."), true);
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
        m_userService->changePassword(oldPassword, newPassword);
    }
}

void SettingsController::handleSaveStreamSettings(int qualityIndex, bool keepAspectRatio)
{
    QSettings settings("DUT_PBL4", "RemoteAccessApp");
    settings.setValue("streaming/qualityIndex", qualityIndex);
    settings.setValue("display/keepAspectRatio", keepAspectRatio);

    if (m_view) {
        m_view->showFeedback(QStringLiteral("Đã cập nhật cài đặt truyền phát."), false);
    }
}

void SettingsController::onChangePasswordResult(bool success, const QString &message)
{
    if (m_view) {
        m_view->showFeedback(message, !success);
        if (success) {
            m_view->clearPasswordInputs();
        }
    }
}

void SettingsController::onProfileResult(bool success, const QString &username, const QString &email, const QString &message)
{
    Q_UNUSED(message);
    if (success && m_view) {
        m_view->setProfileInfo(username, email.isEmpty() ? QStringLiteral("Chưa thiết lập") : email);
    }
}
