#pragma once
#include <QWidget>
#include <QList>
#include "Domain/Model/AccountInfo.h"
class QLabel;
class QLineEdit;
class QVBoxLayout;

class AccountPage : public QWidget {
    Q_OBJECT
public:
    explicit AccountPage(QWidget *parent = nullptr);
    void showLoading();
    void updateAccountList(const QList<AccountInfo> &accounts);
    void showError(const QString &message);
    void showLoadError(const QString &message);
    void setActionsEnabled(bool enabled);
signals:
    void requestAddAccount();
    void loadRequested();
    void editAccountRequested(const QString &username);
    void deleteAccountRequested(const QString &username);
public slots:
    void loadData();
private:
    void setupUi();
    void renderAccounts();
    QList<AccountInfo> m_allAccounts;
    QLabel *m_totalAccountsVal = nullptr;
    QLineEdit *m_searchInput = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
    QWidget *m_scrollContent = nullptr;
    QWidget *m_toolbar = nullptr;
    QWidget *m_metrics = nullptr;
    bool m_loading = true;
    QString m_loadError;
};
