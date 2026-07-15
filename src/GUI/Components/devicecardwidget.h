#ifndef DEVICECARDWIDGET_H
#define DEVICECARDWIDGET_H

#include <QWidget>
#include <QString>

class DeviceCardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceCardWidget(const QString& name, const QString& OS, const QString& ip, const QString& status, QWidget *parent = nullptr);
};

#endif
