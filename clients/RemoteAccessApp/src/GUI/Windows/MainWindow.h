#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include "GUI/Components/SidebarWidget.h"
#include "GUI/Components/TopBarWidget.h"

class DevicesPage;
class AccountPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DevicesPage *devicesPage, AccountPage *accountPage, QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void pageSelected(int pageIndex);

private:
    SidebarWidget *sidebar;
    TopBarWidget *topbar;
    QStackedWidget *stackedWidget;

    void setupUi(DevicesPage *devicesPage, AccountPage *accountPage);
};

#endif
