#ifndef REGISTERCONTROLLER_H
#define REGISTERCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QString>

class RegisterWindow;
class AccountService;

class RegisterController : public QObject
{
    Q_OBJECT
public:
    explicit RegisterController(QObject *parent = nullptr);
    ~RegisterController();

    // Hiển thị giao diện đăng ký
    void start(const QString &parentUsername);

signals:
    void accountCreatedSuccessfully();

private slots:
    void handleRegisterRequested(const QString &childUsername, const QString &password);
    void handleAccountCreated(bool success, const QString &message);

private:
    QPointer<RegisterWindow> m_registerWindow;
    AccountService *m_accountService;
    QString m_parentUsername;
};

#endif // REGISTERCONTROLLER_H
