#include <QApplication>
#include <QFile>
#include <QStringList>
#include "GUI/mainwindow.h"
#include "GUI/loginwindow.h"
#include "Network/HeartbeatReporter.h"
#include <QDebug>

static QString loadStyleSheet()
{
    QString styleSheet;
    const QStringList styleFiles = {
        ":/styles/Resources/styles/main.qss",
        ":/styles/Resources/styles/sidebar.qss",
        ":/styles/Resources/styles/topbar.qss",
        ":/styles/Resources/styles/devicecard.qss",
        ":/styles/Resources/styles/loginwindow.qss",
    };

    for (const QString &styleFile : styleFiles) {
        QFile file(styleFile);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            styleSheet.append(QString::fromUtf8(file.readAll()));
            styleSheet.append('\n');
        }
    }

    return styleSheet;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyleSheet(loadStyleSheet());

    LoginWindow* loginWin = new LoginWindow();

    QObject::connect(loginWin, &LoginWindow::loginSuccessful, [loginWin](bool Success, bool isAdmin, const QString &Message){
        if(isAdmin)
        {
            // ── ADMIN flow: unchanged ──────────────────────────────────────
            MainWindow* mainWin = new MainWindow();
            mainWin->show();
            loginWin->close();
        }
        else
        {
            // ── CHILD flow ─────────────────────────────────────────────────
            // Keep the Qt event loop alive after LoginWindow closes.
            // Without this, QApplication would exit as soon as the last
            // visible window is destroyed.
            qApp->setQuitOnLastWindowClosed(false);

            // Retrieve the authenticated username before closing the window.
            const QString childUsername = loginWin->username();

            // Create HeartbeatReporter owned by qApp so it outlives LoginWindow.
            HeartbeatReporter *reporter = new HeartbeatReporter(qApp);
            reporter->start(childUsername);

            loginWin->close();

            qDebug() << "[main] CHILD login: heartbeat started for user=" << childUsername
                     << " – application running in background.";

            // TODO (future subphase): launch a system-tray / background Host
            // application here. For now the process keeps running silently
            // while HeartbeatReporter reports online status.
        }
    });

    loginWin->show();

    return app.exec();
}
