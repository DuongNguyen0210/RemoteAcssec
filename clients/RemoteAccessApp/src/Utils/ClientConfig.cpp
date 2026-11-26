#include "ClientConfig.h"

#include <QDir>
#include <QFile>
#include <QHash>

namespace {
QHash<QString, QString> fileSettings;
}

void ClientConfig::loadFromDirectory(const QString &directory)
{
    fileSettings.clear();
    QFile file(QDir(directory).filePath(QStringLiteral(".env")));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    auto text = QString::fromUtf8(file.readAll());
    if (text.startsWith(QChar::ByteOrderMark)) text.remove(0, 1);
    for (const auto &entry : text.split(QLatin1Char('\n'))) {
        const auto separator = entry.indexOf(QLatin1Char('='));
        if (separator < 1) continue;
        const auto name = entry.left(separator).trimmed();
        if (name == QStringLiteral("REMOTE_API_URL") || name == QStringLiteral("REMOTE_RELAY_MODE")
            || name == QStringLiteral("REMOTE_RELAY_TLS")) {
            fileSettings.insert(name, entry.mid(separator + 1).trimmed());
        }
    }
}

QString ClientConfig::value(const char *name, const char *defaultValue)
{
    const auto environmentValue = qEnvironmentVariable(name);
    if (!environmentValue.isNull()) return environmentValue;
    return fileSettings.value(QString::fromLatin1(name), QString::fromLatin1(defaultValue));
}
