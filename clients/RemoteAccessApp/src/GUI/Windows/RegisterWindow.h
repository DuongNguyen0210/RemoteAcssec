#ifndef REGISTERWINDOW_H
#define REGISTERWINDOW_H

#include <QWidget>

#include <QString>

class QLineEdit;
class QLabel;

class RegisterWindow : public QWidget
{
    Q_OBJECT
public:
    explicit RegisterWindow(QWidget * parent = nullptr);
    ~RegisterWindow();

    void showError(const QString &msg);
    void showSuccess(const QString &msg);

signals:
    void registerRequested(const QString &username, const QString &password);

private slots:
    void onRegisterClicked();

private:
    void setupUI();

    QLineEdit *m_usernameInput;
    QLineEdit *m_passwordInput;
    QLabel *m_statusLabel;
};

#endif
