#include <QtTest>
#include <QTcpServer>
#include <QNetworkReply>
#include <QScopeGuard>
#include "Network/Http/ApiClient.h"
#include "Network/Http/TlsConfiguration.h"
#include "Network/Relay/RelayClient.h"

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
        QCOMPARE(remoteTlsConfiguration().peerVerifyMode(), QSslSocket::VerifyPeer);
        QCOMPARE(remoteApiTransportAllowed(QUrl("http://127.0.0.1")), !TransportPolicy::requiresTls);
        QVERIFY(!remoteApiTransportAllowed(QUrl("http://example.invalid")));
        qputenv("REMOTE_ALLOW_HTTP", "1");
        QCOMPARE(remoteApiTransportAllowed(QUrl("http://example.invalid")), !TransportPolicy::requiresTls);
        qputenv("REMOTE_CA_FILE", "missing-ca.pem");
        qputenv("REMOTE_API_CA_CERT", ""); // Empty override must retain global trust.
        QVERIFY(!remoteApiTransportAllowed(QUrl("http://127.0.0.1")));
        QVERIFY(!remoteApiTransportAllowed(QUrl("http://example.invalid")));
        QVERIFY(remoteApiTransportAllowed(QUrl("https://example.invalid")));
    }

    void packagedRelayRejectsPlaintextOverride() {
        if (!TransportPolicy::requiresTls) QSKIP("Distribution build only");
        const auto previous = qgetenv("REMOTE_RELAY_TLS");
        const auto restore = qScopeGuard([&] {
            if (previous.isNull()) qunsetenv("REMOTE_RELAY_TLS");
            else qputenv("REMOTE_RELAY_TLS", previous);
        });
        qputenv("REMOTE_RELAY_TLS", "off");
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        RelayClient relay;
        QSignalSpy errors(&relay, &RelayClient::transportError);
        relay.ConnectToServer("127.0.0.1", server.serverPort());
        QCOMPARE(errors.count(), 1);
        QVERIFY(!server.hasPendingConnections());
    }

    void livePublicIpv6ApiAndRelays() {
        const auto address = qEnvironmentVariable("TEST_PUBLIC_IPV6");
        if (address.isEmpty()) QSKIP("Set TEST_PUBLIC_IPV6 for read-only public TLS checks");
        QCOMPARE(QHostAddress(address).protocol(), QAbstractSocket::IPv6Protocol);
        const auto previous = qgetenv("REMOTE_RELAY_TLS");
        const auto previousCa = qgetenv("REMOTE_RELAY_CA_CERT");
        const auto restore = qScopeGuard([&] {
            if (previous.isNull()) qunsetenv("REMOTE_RELAY_TLS");
            else qputenv("REMOTE_RELAY_TLS", previous);
            if (previousCa.isNull()) qunsetenv("REMOTE_RELAY_CA_CERT");
            else qputenv("REMOTE_RELAY_CA_CERT", previousCa);
        });
        qputenv("REMOTE_RELAY_TLS", "required");
        qunsetenv("REMOTE_RELAY_CA_CERT");
        auto &api = ApiClient::instance();
        api.setBaseUrl(QStringLiteral("https://[%1]").arg(address));
        api.setToken({});
        auto *reply = api.get("/api/v1/devices");
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 12000);
        QVERIFY2(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401,
                 qPrintable(reply->errorString()));
        reply->deleteLater();

        for (const quint16 port : {9091, 9092}) {
            RelayClient relay;
            QSignalSpy connected(&relay, &RelayClient::connected);
            QSignalSpy errors(&relay, &RelayClient::transportError);
            relay.ConnectToServer(address, port);
            QTRY_VERIFY_WITH_TIMEOUT(!connected.isEmpty() || !errors.isEmpty(), 10000);
            QVERIFY2(!connected.isEmpty(), errors.isEmpty() ? "Relay did not complete TLS"
                                                           : qPrintable(errors.first().first().toString()));
            relay.DisconnectFromServer();
        }
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
