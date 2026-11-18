#ifndef TOPBARWIDGET_H
#define TOPBARWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QString>

class TopBarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TopBarWidget(QWidget *parent = nullptr);
    void clearSearch();
signals:
    void searchChanged(const QString &query);

private:
    void setupUi();
    QLineEdit *searchInput = nullptr;
};

#endif
