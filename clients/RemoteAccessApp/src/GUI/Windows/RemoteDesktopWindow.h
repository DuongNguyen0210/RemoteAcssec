#ifndef REMOTEDESKTOPWINDOW_H
#define REMOTEDESKTOPWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>

class RemoteScreenWidget;

class RemoteDesktopWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RemoteDesktopWindow(quint64 sessionId, const QString &targetName, QWidget *parent = nullptr);
    ~RemoteDesktopWindow() override = default;

    void updateFrame(const QImage &image);
    void closeForSessionEnd();
    void setSessionInfo(quint64 sessionId, const QString &targetName);
    void updateStatus(const QString &statusText);

signals:
    void disconnectRequested();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onFpsUpdated(int fps);
    void onResolutionChanged(const QSize &res);
    void onDisconnectClicked();

private:
    void setupUi();

    quint64 m_sessionId;
    QString m_targetName;

    RemoteScreenWidget *m_screenWidget = nullptr;
    QLabel *m_lblTitle = nullptr;
    QLabel *m_lblStats = nullptr;
    QPushButton *m_btnDisconnect = nullptr;

    bool m_inCloseEvent = false;
    bool m_disconnectEmitted = false;
    int m_lastFps = 0;
    QSize m_lastResolution;
};

#endif // REMOTEDESKTOPWINDOW_H
