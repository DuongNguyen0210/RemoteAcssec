#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include "Components/sidebarwidget.h"
#include "Components/topbarwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    SidebarWidget *sidebar;
    TopBarWidget *topbar;
    QStackedWidget *stackedWidget;

    void setupUi();
};

#endif
