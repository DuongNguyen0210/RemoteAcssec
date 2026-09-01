#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include "GUI/Widgets/sidebarwidget.h"
#include "GUI/Widgets/topbarwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void requestAddAccount();
    void childConnectRequested(const QString &childUsername);

private:
    SidebarWidget *sidebar;
    TopBarWidget *topbar;
    QStackedWidget *stackedWidget;

    void setupUi();
};

#endif
