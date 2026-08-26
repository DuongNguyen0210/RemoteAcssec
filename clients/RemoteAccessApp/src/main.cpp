#include <QApplication>
#include <QFile>
#include <QStringList>
#include "GUI/mainwindow.h"
#include "GUI/loginwindow.h"
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
    loginWin->setAttribute(Qt::WA_QuitOnClose, false);
    MainWindow* mainWin = new MainWindow();

    QObject::connect(loginWin, &LoginWindow::loginSuccessful, [loginWin, mainWin](bool Success, bool isAdmin, const QString &Message){
        loginWin->close();
        if(isAdmin)
            mainWin->show();
        else
        {
            //TODO: mo mot app chay ngam cho sub accout
        }
    });

    loginWin->show();

    return app.exec();
}
