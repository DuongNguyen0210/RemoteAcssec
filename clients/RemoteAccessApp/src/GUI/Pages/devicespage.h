#ifndef DEVICESPAGE_H
#define DEVICESPAGE_H

#include <QWidget>

class DevicesPage : public QWidget
{
    Q_OBJECT
public:
    explicit DevicesPage(QWidget *parent = nullptr);
private:
    void setupUi();

};

#endif
