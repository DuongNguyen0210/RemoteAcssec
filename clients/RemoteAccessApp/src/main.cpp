#include <QApplication>
#include <QFile>
#include <QStringList>
#include <QDebug>
#include "Core/Controllers/appcontroller.h"

static QString loadStyleSheet()
{
    QString styleSheet;
    const QStringList styleFiles = {
        ":/styles/Resources/styles/main.qss",
        ":/styles/Resources/styles/sidebar.qss",
        ":/styles/Resources/styles/topbar.qss",
        ":/styles/Resources/styles/devicecard.qss",
        ":/styles/Resources/styles/loginwindow.qss",
        ":/styles/Resources/styles/registerwindow.qss",
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

    // Khởi tạo AppController làm bộ điều phối trung tâm
    AppController controller;
    controller.start();

    return app.exec();
}
