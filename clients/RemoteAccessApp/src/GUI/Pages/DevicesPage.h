#ifndef DEVICESPAGE_H
#define DEVICESPAGE_H

#include <QWidget>
#include <QStringList>

#include <QJsonArray>

class AccountService;
class FlowLayout;
class QWidget;

class DevicesPage : public QWidget
{
    Q_OBJECT
public:
    explicit DevicesPage(QWidget *parent = nullptr);
    void loadDevices();

signals:
    void connectRequested(const QString &childUsername);

private slots:
    void handleChildrenLoaded(bool success, const QJsonArray &children, const QString &message);

private:
    void setupUi();

    AccountService *m_accountService;
    FlowLayout *m_flowLayout;
    QWidget *m_scrollContent;
};

#endif
