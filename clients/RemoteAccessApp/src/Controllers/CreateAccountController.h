#ifndef CREATEACCOUNTCONTROLLER_H
#define CREATEACCOUNTCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QString>

class CreateAccountDialog;
class AccountService;

class CreateAccountController : public QObject
{
    Q_OBJECT
public:
    explicit CreateAccountController(QObject *parent = nullptr);
    ~CreateAccountController();

    void start(const QString &parentUsername);

signals:
    void accountCreatedSuccessfully();
    void finished();

private slots:
    void handleRegisterRequested(const QString &childUsername, const QString &password);
    void handleAccountCreated(bool success, const QString &message);

private:
    QPointer<CreateAccountDialog> m_dialog;
    AccountService *m_accountService;
    QString m_parentUsername;
};

#endif
