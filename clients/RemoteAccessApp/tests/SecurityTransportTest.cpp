#include <QtTest>
#include <QTcpServer>
#include <QNetworkReply>
#include <QScopeGuard>
#include "Network/Http/ApiClient.h"
#include "Network/Http/TlsConfiguration.h"

class SecurityTransportTest : public QObject {
    Q_OBJECT
private slots:
    void init() {
        for (const char *name : {"REMOTE_CA_FILE", "REMOTE_API_CA_CERT", "REMOTE_ALLOW_HTTP"}) qunsetenv(name);
        ApiClient::instance().setToken("sentinel-token-never-send-plaintext");
        ApiClient::instance().getManager()->clearConnectionCache();
    }
    void cleanup() { init(); }

    void tlsPolicyAndExplicitLabOverride() {
        QCOMPARE(remoteTlsConfiguration().protocol(), QSsl::TlsV1_2OrLater);
        QVERIFY(remoteApiTransportAllowed(QUrl("http://127.0.0.1")));
        QVERIFY(!remoteApiTransportAllowed(QUrl("http://example.invalid")));
        qputenv("REMOTE_ALLOW_HTTP", "1");
        QVERIFY(remoteApiTransportAllowed(QUrl("http://example.invalid")));
        qputenv("REMOTE_CA_FILE", "missing-ca.pem");
        qputenv("REMOTE_API_CA_CERT", ""); // Empty override must retain global trust.
        QVERIFY(!remoteApiTransportAllowed(QUrl("http://127.0.0.1")));
        QVERIFY(!remoteApiTransportAllowed(QUrl("http://example.invalid")));
        QVERIFY(remoteApiTransportAllowed(QUrl("https://example.invalid")));
    }

    void configuredTrustNeverSendsTokenOverHttp_data() {
        QTest::addColumn<bool>("stream");
        QTest::newRow("REST") << false;
        QTest::newRow("SSE") << true;
    }
    void configuredTrustNeverSendsTokenOverHttp() {
        QFETCH(bool, stream);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QSignalSpy connections(&server, &QTcpServer::newConnection);
        qputenv("REMOTE_API_CA_CERT", "missing-ca.pem");
        qputenv("REMOTE_ALLOW_HTTP", "1");
        auto &api = ApiClient::instance();
        api.setBaseUrl("http://127.0.0.1:" + QString::number(server.serverPort()));
        auto *reply = stream ? api.openEventStream("/api/v1/devices/events") : api.get("/api/v1/user/profile");
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(reply->error() != QNetworkReply::NoError);
        QCOMPARE(connections.count(), 0);
        reply->deleteLater();
    }
    void invalidCaFailsBeforeConnecting() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QSignalSpy connections(&server, &QTcpServer::newConnection);
        qputenv("REMOTE_CA_FILE", "missing-ca.pem");
        auto &api = ApiClient::instance();
        api.setBaseUrl("https://localhost:" + QString::number(server.serverPort()));
        auto *reply = api.get("/api/v1/user/profile");
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(reply->url().isEmpty());
        QCOMPARE(connections.count(), 0);
        reply->deleteLater();
    }
    void liveHttpsCertificateAndEventStream_data() {
        QTest::addColumn<bool>("stream");
        QTest::addColumn<QString>("trust");
        QTest::addColumn<bool>("wrongHost");
        QTest::newRow("REST-trusted") << false << "trusted" << false;
        QTest::newRow("REST-untrusted") << false << "system" << false;
        QTest::newRow("REST-wrong-host") << false << "trusted" << true;
        QTest::newRow("SSE-trusted") << true << "trusted" << false;
        QTest::newRow("SSE-untrusted") << true << "system" << false;
        QTest::newRow("SSE-wrong-host") << true << "trusted" << true;
    }
    void liveHttpsCertificateAndEventStream() {
        if (qEnvironmentVariableIsEmpty("TEST_HTTPS_API_URL") || qEnvironmentVariableIsEmpty("TEST_HTTPS_CA_CERT"))
            QSKIP("Set TEST_HTTPS_API_URL and TEST_HTTPS_CA_CERT for live HTTPS tests");
        QFETCH(bool, stream); QFETCH(QString, trust); QFETCH(bool, wrongHost);
        if (trust == "trusted") qputenv("REMOTE_API_CA_CERT", qgetenv("TEST_HTTPS_CA_CERT"));
        QUrl url(qEnvironmentVariable("TEST_HTTPS_API_URL"));
        if (wrongHost) url.setHost("127.0.0.1"); // Lab cert intentionally has only DNS:localhost.
        auto &api = ApiClient::instance();
        api.setBaseUrl(url.toString());
        api.setToken(QString());
        auto *reply = stream ? api.openEventStream("/api/v1/devices/events") : api.get("/api/v1/user/profile");
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (trust == "trusted" && !wrongHost) {
            QCOMPARE(status, 401); // TLS succeeded; anonymous REST/SSE must still be rejected.
        } else {
            QCOMPARE(status, 0);
            QCOMPARE(reply->error(), QNetworkReply::SslHandshakeFailedError);
        }
        reply->deleteLater();
    }
};
QTEST_GUILESS_MAIN(SecurityTransportTest)
#include "SecurityTransportTest.moc"
