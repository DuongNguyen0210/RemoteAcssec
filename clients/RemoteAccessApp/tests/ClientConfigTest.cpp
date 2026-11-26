#include <QtTest>
#include <QFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QUrl>
#include "Utils/ClientConfig.h"

class ClientConfigTest : public QObject {
    Q_OBJECT
private slots:
    void executableDirectoryAndPrecedence() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto previous = qgetenv("REMOTE_API_URL");
        const auto restore = qScopeGuard([&] {
            if (previous.isNull()) qunsetenv("REMOTE_API_URL");
            else qputenv("REMOTE_API_URL", previous);
            ClientConfig::loadFromDirectory(directory.filePath("unused"));
        });
        qunsetenv("REMOTE_API_URL");
        ClientConfig::loadFromDirectory(directory.path());
        QCOMPARE(ClientConfig::value("REMOTE_API_URL", "https://localhost"), QString("https://localhost"));

        QFile file(directory.filePath(".env"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray settings = "\xef\xbb\xbf" "# release settings\r\n"
            "REMOTE_API_URL = https://[2001:db8::1234]:443\r\n"
            "REMOTE_RELAY_MODE=dynamic\r\nREMOTE_RELAY_TLS=required\r\n"
            "JWT_SECRET=should-not-be-read\r\nREMOTE_ALLOW_HTTP=1\r\n";
        QCOMPARE(file.write(settings), settings.size());
        file.close();
        ClientConfig::loadFromDirectory(directory.path());

        const auto endpoint = ClientConfig::value("REMOTE_API_URL", "");
        QCOMPARE(endpoint, QString("https://[2001:db8::1234]:443"));
        const QUrl url(endpoint, QUrl::StrictMode);
        QVERIFY(url.isValid());
        QCOMPARE(url.host(), QString("2001:db8::1234"));
        QCOMPARE(ClientConfig::value("REMOTE_RELAY_MODE", "static"),
                 qEnvironmentVariable("REMOTE_RELAY_MODE", "dynamic"));
        QCOMPARE(ClientConfig::value("REMOTE_RELAY_TLS", "off"),
                 qEnvironmentVariable("REMOTE_RELAY_TLS", "required"));
        QCOMPARE(ClientConfig::value("JWT_SECRET", "unused"), qEnvironmentVariable("JWT_SECRET", "unused"));
        QCOMPARE(ClientConfig::value("REMOTE_ALLOW_HTTP", "0"), qEnvironmentVariable("REMOTE_ALLOW_HTTP", "0"));

        qputenv("REMOTE_API_URL", "https://remote.example.com");
        QCOMPARE(ClientConfig::value("REMOTE_API_URL", ""), QString("https://remote.example.com"));
    }
};

QTEST_GUILESS_MAIN(ClientConfigTest)
#include "ClientConfigTest.moc"
