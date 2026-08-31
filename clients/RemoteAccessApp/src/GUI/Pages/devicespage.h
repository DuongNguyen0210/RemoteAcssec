#ifndef DEVICESPAGE_H
#define DEVICESPAGE_H

#include <QWidget>
#include <QStringList>

class ChildDiscoveryService;
class FlowLayout;
class QWidget;

class DevicesPage : public QWidget
{
    Q_OBJECT
public:
    explicit DevicesPage(QWidget *parent = nullptr);

signals:
    void connectRequested(const QString &childUsername);

private slots:
    void handleChildrenLoaded(const QStringList &childUsernames);
    void handleLoadFailed(const QString &message);

private:
    void setupUi();

    ChildDiscoveryService *m_childDiscoveryService;
    FlowLayout *m_flowLayout;
    QWidget *m_scrollContent;
};

#endif
