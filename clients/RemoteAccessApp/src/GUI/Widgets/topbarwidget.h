#ifndef TOPBARWIDGET_H
#define TOPBARWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QString>

class TopBarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TopBarWidget(QWidget *parent = nullptr);
    void setSearchPlaceholder(const QString &placeholder);

private:
    void setupUi();
    QLineEdit *searchInput = nullptr;
};

#endif
