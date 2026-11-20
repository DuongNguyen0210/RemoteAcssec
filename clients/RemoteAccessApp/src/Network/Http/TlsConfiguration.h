#pragma once
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QHostAddress>
#include <QUrl>

inline QSslConfiguration remoteTlsConfiguration()
{
    auto config = QSslConfiguration::defaultConfiguration();
    config.setProtocol(QSsl::TlsV1_2OrLater);
    const auto path = qEnvironmentVariable("REMOTE_CA_FILE");
    if (!path.isEmpty()) {
        // An explicit trust file replaces the system roots for this deployment.
        config.setCaCertificates(QSslCertificate::fromPath(path));
    }
    return config;
}

inline bool remoteApiTransportAllowed(const QUrl &url)
{
    return url.scheme() == "https" || (url.scheme() == "http"
        && (url.host() == "localhost" || QHostAddress(url.host()).isLoopback()));
}
