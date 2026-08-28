#ifndef ACCOUNTSERVICE_H
#define ACCOUNTSERVICE_H

#include <QObject>
#include <QString>
#include <QNetworkReply>

class AccountService : public QObject
{
    Q_OBJECT
public:
    explicit AccountService(QObject *parent = nullptr);

    // Hàm tạo tài khoản con mới
    // Truyền vào thông tin tài khoản con, và tên của tài khoản cha (admin)
    void createSubAccount(const QString &childUsername, const QString &password, const QString &parentUsername);

signals:
    // Tín hiệu báo kết quả về cho Controller hoặc GUI
    void createAccountResult(bool success, const QString &message);

private slots:
    void onCreateAccountReply(QNetworkReply *reply);
};

#endif // ACCOUNTSERVICE_H
