#ifndef ACCOUNTPAGE_H
#define ACCOUNTPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QList>
#include <QString>
#include "Domain/Model/DeviceInfo.h"

class QLabel;
class QLineEdit;

class AccountPage : public QWidget
{
    Q_OBJECT
public:
    explicit AccountPage(QWidget *parent = nullptr);

    void showLoading();
    void updateAccountList(const QList<DeviceInfo> &accounts);
    void showError(const QString &message);

signals:
    void requestAddAccount();
    void loadRequested();

public slots:
    void loadData();

private slots:
    void onSearchTextChanged(const QString &text);

private:
    void setupUi();
    void updateMetrics();
    void renderAccounts(const QString &filterText);

    QList<DeviceInfo> m_allAccounts;

    QLabel *m_totalAccountsVal;
    QLabel *m_activeAccountsVal;
    QLabel *m_inactiveAccountsVal;

    QLineEdit *m_searchInput;
    QVBoxLayout *m_listLayout;
    QWidget *m_scrollContent;
};

#endif
