#ifndef ACCOUNTPAGE_H
#define ACCOUNTPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QString>

class AccountController;

class AccountPage : public QWidget
{
    Q_OBJECT
public:
    explicit AccountPage(QWidget *parent = nullptr);
    
    void setController(AccountController *controller);
    
    void showLoading();
    void updateAccountList(const QJsonArray &children);
    void showError(const QString &message);

signals:
    void requestAddAccount();
    void requestEditAccount();
    void requestDeleteAccount();

public slots:
    void loadData();

private:
    void setupUi();

    AccountController *m_controller;
    QVBoxLayout *m_listLayout;
    QWidget *m_scrollContent;
};

#endif // ACCOUNTPAGE_H
