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
#if defined(Q_OS_LINUX)
    // On Linux desktop, QtWayland disables QScreen::grabWindow(0) for security.
    // Defaulting to xcb (XWayland) if DISPLAY is available allows reliable screen capture.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && !qEnvironmentVariableIsEmpty("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Enable High-DPI scaling for crisp rendering across Windows & Linux multi-monitor displays
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
