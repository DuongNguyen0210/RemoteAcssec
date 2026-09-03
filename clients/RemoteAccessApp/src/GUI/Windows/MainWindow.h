#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include "../Components/SidebarWidget.h"
#include "../Components/TopbarWidget.h"

class DevicesPage;
class AccountPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DevicesPage *devicesPage, AccountPage *accountPage, QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void requestAddAccount();
    void childConnectRequested(const QString &childUsername);
    void pageSelected(int pageIndex);

private:
    SidebarWidget *sidebar;
    TopBarWidget *topbar;
    QStackedWidget *stackedWidget;

    void setupUi(DevicesPage *devicesPage, AccountPage *accountPage);
};

#endif
