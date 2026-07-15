#ifndef TOPBARWIDGET_H
#define TOPBARWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>

class TopBarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TopBarWidget(QWidget *parent = nullptr);
private:
    void setupUi();
};

#endif
