#pragma once

#include <QHostAddress>
#include <QString>

namespace TransportPolicy {
inline bool isLoopback(const QString &host)
{
    return host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0
        || QHostAddress(host).isLoopback();
}
}
