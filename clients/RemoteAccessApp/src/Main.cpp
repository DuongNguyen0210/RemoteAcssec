#include <QApplication>
#include <QFile>
#include <QStringList>
#include <QDebug>
#include <QTranslator>
#include <QLocale>
#include "Controllers/AppCoordinator.h"

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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Hỗ trợ hiển thị sắc nét trên màn hình High-DPI (Windows 2K/4K/Surface scaling)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::Vietnamese, QLocale::Vietnam));
    QTranslator inputTranslation;
    if (inputTranslation.load(":/translations/qt_vi.qm")) app.installTranslator(&inputTranslation);
    app.setStyleSheet(loadStyleSheet());

    AppCoordinator coordinator;
    coordinator.start();

    return app.exec();
}
