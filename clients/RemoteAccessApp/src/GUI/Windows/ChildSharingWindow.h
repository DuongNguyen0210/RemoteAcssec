#pragma once

#include <QWidget>

class QCloseEvent;
class QLabel;
class QPushButton;

class ChildSharingWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChildSharingWindow(QWidget *parent = nullptr);
    void sessionStarted();
    void sessionEnded();
    void authenticationLost();

signals:
    void sharingEnabledChanged(bool enabled);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class State { Off, Ready, Sharing, Paused, SignedOut };

    void stopSharing();
    void updateControls();

    State m_state = State::Off;
    QLabel *m_status;
    QPushButton *m_toggle;
};
