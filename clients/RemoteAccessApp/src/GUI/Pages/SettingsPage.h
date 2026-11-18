#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QString>

class QLineEdit;
class QLabel;
class QPushButton;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setProfileInfo(const QString &username, const QString &email);
    void showFeedback(const QString &message, bool isError);
    void clearPasswordInputs();
    void setPasswordChangePending(bool pending);

signals:
    void changePasswordRequested(const QString &oldPassword, const QString &newPassword, const QString &confirmPassword);

private slots:
    void onApplyPasswordClicked();

private:
    void setupUi();

    // Profile & Password
    QLabel    *m_usernameLabel = nullptr;
    QLineEdit *m_oldPasswordInput = nullptr;
    QLineEdit *m_newPasswordInput = nullptr;
    QLineEdit *m_confirmPasswordInput = nullptr;
    QPushButton *m_btnChangePassword = nullptr;

    // Feedback
    QLabel *m_feedbackLabel = nullptr;
};

#endif
