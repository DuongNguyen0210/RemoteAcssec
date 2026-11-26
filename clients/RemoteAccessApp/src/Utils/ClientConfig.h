#pragma once

#include <QString>

namespace ClientConfig {
void loadFromDirectory(const QString &directory);
QString value(const char *name, const char *defaultValue);
}
