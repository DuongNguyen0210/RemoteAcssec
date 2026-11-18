#include "AppTheme.h"
#include <QApplication>
#include <QFile>
#include <QDebug>

void applyAppTheme(QApplication &app)
{
    QFile theme(QStringLiteral(":/styles/Resources/styles/main.qss"));
    if (!theme.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot load desktop stylesheet";
        return;
    }
    app.setStyleSheet(QString::fromUtf8(theme.readAll()));
}
