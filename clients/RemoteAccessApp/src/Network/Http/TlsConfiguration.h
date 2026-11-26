#pragma once
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslSocket>
#include <QHostAddress>
#include <QUrl>
#include "../TransportPolicy.h"

inline QString remoteCaFile(const char *name)
{
    const auto specific = qEnvironmentVariable(name);
    return specific.isEmpty() ? qEnvironmentVariable("REMOTE_CA_FILE") : specific;
}

inline QSslConfiguration remoteTlsConfiguration(const char *trustVariable = "REMOTE_CA_FILE")
{
    auto config = QSslConfiguration::defaultConfiguration();
    config.setProtocol(QSsl::TlsV1_2OrLater);
    config.setPeerVerifyMode(QSslSocket::VerifyPeer);
    const auto path = remoteCaFile(trustVariable);
    if (!path.isEmpty()) {
        // An explicit trust file replaces the system roots for this deployment.
        config.setCaCertificates(QSslCertificate::fromPath(path));
    }
    return config;
}

inline bool remoteApiTransportAllowed(const QUrl &url)
{
    if (url.scheme() == "https") return true;
    if (TransportPolicy::requiresTls) return false;
    // A configured trust file is an explicit TLS choice, never downgrade to HTTP.
    if (!remoteCaFile("REMOTE_API_CA_CERT").isEmpty()) return false;
    return url.scheme() == "http"
        && (TransportPolicy::isLoopback(url.host()) || qgetenv("REMOTE_ALLOW_HTTP") == "1");
}
