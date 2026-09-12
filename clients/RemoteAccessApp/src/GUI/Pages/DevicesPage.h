#ifndef DEVICESPAGE_H
#define DEVICESPAGE_H

#include <QWidget>
#include <QList>
#include "Domain/Model/DeviceInfo.h"

class FlowLayout;
class QWidget;

class DevicesPage : public QWidget
{
    Q_OBJECT
public:
    explicit DevicesPage(QWidget *parent = nullptr);

    void updateDeviceList(const QList<DeviceInfo> &devices);
    void showError(const QString &message);
    void showSuccess(const QString &message);

signals:
    void connectRequested(const QString &childUsername);
    void removeDeviceRequested(const QString &childUsername);
    void refreshRequested();

private:
    void setupUi();

    FlowLayout *m_flowLayout;
    QWidget *m_scrollContent;
};

#endif
