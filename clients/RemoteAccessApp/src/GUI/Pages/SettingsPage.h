#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QString>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setProfileInfo(const QString &username, const QString &email);
    void setServerUrl(const QString &url);
    void setStreamSettings(int qualityIndex, bool keepAspectRatio);
    void showFeedback(const QString &message, bool isError);
    void clearPasswordInputs();

signals:
    void changePasswordRequested(const QString &oldPassword, const QString &newPassword, const QString &confirmPassword);
    void saveServerRequested(const QString &apiUrl);
    void resetServerRequested();
    void saveStreamSettingsRequested(int qualityIndex, bool keepAspectRatio);

private slots:
    void onApplyPasswordClicked();
    void onSaveServerClicked();
    void onResetServerClicked();
    void onSaveStreamClicked();

private:
    void setupUi();

    // Profile & Password
    QLabel    *m_usernameLabel = nullptr;
    QLabel    *m_emailLabel    = nullptr;
    QLineEdit *m_oldPasswordInput = nullptr;
    QLineEdit *m_newPasswordInput = nullptr;
    QLineEdit *m_confirmPasswordInput = nullptr;
    QPushButton *m_btnChangePassword = nullptr;

    // Server
    QLineEdit *m_serverUrlInput = nullptr;
    QPushButton *m_btnSaveServer = nullptr;
    QPushButton *m_btnResetServer = nullptr;

    // Streaming
    QComboBox *m_qualityCombo = nullptr;
    QCheckBox *m_keepAspectCheck = nullptr;
    QPushButton *m_btnSaveStream = nullptr;

    // Feedback
    QLabel *m_feedbackLabel = nullptr;
};

#endif
