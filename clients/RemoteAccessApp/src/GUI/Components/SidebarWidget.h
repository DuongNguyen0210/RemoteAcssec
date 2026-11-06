#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include <QWidget>
#include <QPushButton>

class QButtonGroup;

class SidebarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SidebarWidget(QWidget *parent = nullptr);
    void selectPage(int index);

signals:
    void pageChanged(int index);

private:
    void setupUi();
    QButtonGroup *menuGroup = nullptr;
};

#endif
