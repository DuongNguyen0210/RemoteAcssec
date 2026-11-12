#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include "GUI/Components/SidebarWidget.h"
#include "GUI/Components/TopBarWidget.h"

class DevicesPage;
class AccountPage;
class SettingsPage;
class SessionsPage;
class QImage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DevicesPage *devicesPage, AccountPage *accountPage, SettingsPage *settingsPage = nullptr, QWidget *parent = nullptr);
    ~MainWindow();
    void showRemoteSession();
    void setRemoteFrame(const QImage &image);
    void clearRemoteFrame();

signals:
    void pageSelected(int pageIndex);
    void disconnectRequested();

private:
    SidebarWidget *sidebar;
    TopBarWidget *topbar;
    QStackedWidget *stackedWidget;
    SessionsPage *pageSessions;

    void setupUi(DevicesPage *devicesPage, AccountPage *accountPage, SettingsPage *settingsPage);
};

#endif
