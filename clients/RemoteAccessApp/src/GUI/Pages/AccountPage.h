#ifndef ACCOUNTPAGE_H
#define ACCOUNTPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QList>
#include <QString>
#include "Domain/Model/AccountInfo.h"

class QLabel;
class QLineEdit;

class AccountPage : public QWidget
{
    Q_OBJECT
public:
    explicit AccountPage(QWidget *parent = nullptr);

    void showLoading();
    void updateAccountList(const QList<AccountInfo> &accounts);
    void showError(const QString &message);

signals:
    void requestAddAccount();
    void loadRequested();
    void editAccountRequested(const QString &username);
    void deleteAccountRequested(const QString &username);

public slots:
    void loadData();

private slots:
    void onSearchTextChanged(const QString &text);

private:
    void setupUi();
    void updateMetrics();
    void renderAccounts(const QString &filterText);

    QList<AccountInfo> m_allAccounts;

    QLabel *m_totalAccountsVal;

    QLineEdit *m_searchInput;
    QVBoxLayout *m_listLayout;
    QWidget *m_scrollContent;
};

#endif
