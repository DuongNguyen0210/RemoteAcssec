#ifndef ACCOUNTPAGE_H
#define ACCOUNTPAGE_H

#include <QWidget>

class AccountPage : public QWidget
{
    Q_OBJECT
public:
    explicit AccountPage(QWidget *parent = nullptr);

private:
    void setupUi();
};

#endif // ACCOUNTPAGE_H
