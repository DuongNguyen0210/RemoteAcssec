#ifndef ACCOUNTPAGE_H
#define ACCOUNTPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QList>
#include <QString>
#include "../../Core/Model/DeviceInfo.h"

class AccountController;

class AccountPage : public QWidget
{
    Q_OBJECT
public:
    explicit AccountPage(QWidget *parent = nullptr);
    
    void setController(AccountController *controller);
    
    void showLoading();
    void updateAccountList(const QList<DeviceInfo> &accounts);
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
