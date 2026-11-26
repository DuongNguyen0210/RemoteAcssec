#pragma once

#include <QHostAddress>
#include <QString>

namespace TransportPolicy {
#ifdef REMOTEACCESS_REQUIRE_TLS
inline constexpr bool requiresTls = true;
#else
inline constexpr bool requiresTls = false;
#endif

inline bool isLoopback(const QString &host)
{
    return host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0
        || QHostAddress(host).isLoopback();
}
}
