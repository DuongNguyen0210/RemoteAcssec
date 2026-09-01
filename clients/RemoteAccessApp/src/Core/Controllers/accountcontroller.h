#ifndef ACCOUNTCONTROLLER_H
#define ACCOUNTCONTROLLER_H

#include <QObject>
#include <QJsonArray>
#include <QString>

class AccountPage;
class AccountService;

class AccountController : public QObject
{
    Q_OBJECT
public:
    explicit AccountController(QObject *parent = nullptr);
    ~AccountController();

    AccountPage* getView() const;

public slots:
    // GUI sẽ gọi hàm này khi cần load dữ liệu
    void fetchAccounts();

private slots:
    // Xử lý kết quả từ Service
    void onFetchResult(bool success, const QJsonArray &children, const QString &message);
    void onAddAccountRequested();

signals:
    void requestAddAccount(); // Bắn lên MainWindow/AppController

private:
    AccountPage *m_view;
    AccountService *m_service;
};

#endif // ACCOUNTCONTROLLER_H
