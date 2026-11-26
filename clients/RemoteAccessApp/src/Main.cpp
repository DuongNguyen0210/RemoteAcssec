#include <QApplication>
#include "GUI/AppTheme.h"
#include <QTranslator>
#include <QLocale>
#include "Controllers/AppCoordinator.h"
#include "Utils/ClientConfig.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Hỗ trợ hiển thị sắc nét trên màn hình High-DPI (Windows 2K/4K/Surface scaling)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    ClientConfig::loadFromDirectory(QCoreApplication::applicationDirPath());
    QLocale::setDefault(QLocale(QLocale::Vietnamese, QLocale::Vietnam));
    QTranslator inputTranslation;
    if (inputTranslation.load(":/translations/qt_vi.qm")) app.installTranslator(&inputTranslation);
    applyAppTheme(app);

    AppCoordinator coordinator;
    coordinator.start();

    return app.exec();
}
