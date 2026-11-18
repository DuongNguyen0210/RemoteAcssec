#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

// Keeps login alive independently of CHILD screen-sharing/presence.
class LoginSessionMonitor : public QObject {
    Q_OBJECT
public:
    explicit LoginSessionMonitor(QObject *parent = nullptr);
    void start();
    void stop();
    void logout();
signals:
    void sessionLost(const QString &message);
private:
    void renew();
    QTimer m_timer;
    QElapsedTimer m_lastConfirmed;
    bool m_inFlight = false;
    bool m_loggedOut = false;
};
