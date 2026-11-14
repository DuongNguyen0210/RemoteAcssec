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
    void setSearchQuery(const QString &query);
    void showError(const QString &message);
    void showSuccess(const QString &message);
    void showSessionStatus(const QString &message);

signals:
    void connectRequested(const QString &agentSessionId);
    void refreshRequested();
    void clearSearchRequested();

private:
    void setupUi();
    void renderDevices();

    FlowLayout *m_flowLayout;
    QWidget *m_scrollContent;
    class QLabel *m_refreshStatus;
    class QLabel *m_sessionStatus;
    QList<DeviceInfo> m_allDevices;
    QString m_searchQuery;
};

#endif
