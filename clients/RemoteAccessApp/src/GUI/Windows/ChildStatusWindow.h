#pragma once
#include <QWidget>
class QLabel;
class QPushButton;

// Displays sharing state; transport and heartbeat lifecycles belong to the coordinator.
class ChildStatusWindow : public QWidget {
    Q_OBJECT
public:
    explicit ChildStatusWindow(QWidget *parent = nullptr);
    void showRelayReady();
    void showSessionStarted();
    void showSessionEnded(bool relayReady);
    void showRelayFailure();
    void showAuthenticationLost();
    void showSharingEnabled(bool enabled);
signals:
    void sharingToggleRequested();
#ifdef Q_OS_WIN
protected:
    void changeEvent(QEvent *event) override;
#endif
private:
#ifdef Q_OS_WIN
    QPushButton *m_windowSizeButton = nullptr;
#endif
    QLabel *m_status;
    QPushButton *m_toggle;
};
