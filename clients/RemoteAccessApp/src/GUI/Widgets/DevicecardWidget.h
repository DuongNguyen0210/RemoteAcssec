#ifndef DEVICECARDWIDGET_H
#define DEVICECARDWIDGET_H

#include <QWidget>
#include <QString>

class DeviceCardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceCardWidget(const QString &childUsername, const QString &name,
                              const QString &OS, const QString &ip,
                              const QString &status, const QString &uptime,
                              QWidget *parent = nullptr);

signals:
    void connectRequested(const QString &childUsername);

private:
    QString m_childUsername;
};

#endif
