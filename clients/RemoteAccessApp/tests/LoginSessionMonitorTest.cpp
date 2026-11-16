#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include "Network/Http/LoginSessionMonitor.h"
#include "Network/Http/ApiClient.h"

class LoginSessionMonitorTest : public QObject {
    Q_OBJECT
private slots:
    void revokedLoginStopsRenewing() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        ApiClient::instance().setBaseUrl("http://127.0.0.1:" + QString::number(server.serverPort()));
        LoginSessionMonitor monitor;
        QSignalSpy lost(&monitor, &LoginSessionMonitor::sessionLost);
        monitor.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable() > 0);
        QVERIFY(peer->readAll().startsWith("POST /api/v1/auth/heartbeat "));
        peer->write("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        peer->disconnectFromHost();
        QTRY_COMPARE(lost.count(), 1);
        QTest::qWait(5100);
        QVERIFY(!server.hasPendingConnections());
    }
    void stopIgnoresLateAuthenticationFailure() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        ApiClient::instance().setBaseUrl("http://127.0.0.1:" + QString::number(server.serverPort()));
        LoginSessionMonitor monitor;
        QSignalSpy lost(&monitor, &LoginSessionMonitor::sessionLost);
        monitor.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable() > 0);
        peer->readAll();
        monitor.stop();
        peer->write("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        peer->disconnectFromHost();
        QTest::qWait(100);
        QCOMPARE(lost.count(), 0);
    }
    void logoutIsDeliveredOnceBeforeReturning() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        ApiClient::instance().setBaseUrl("http://127.0.0.1:" + QString::number(server.serverPort()));
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *peer = server.nextPendingConnection();
            connect(peer, &QTcpSocket::readyRead, peer, [&, peer] {
                const auto bytes = peer->readAll();
                if (!bytes.startsWith("POST /api/v1/auth/logout ")) return;
                ++requests;
                peer->write("HTTP/1.1 200 OK\r\nContent-Length: 16\r\nConnection: close\r\n\r\n{\"success\":true}");
                peer->disconnectFromHost();
            });
        });
        LoginSessionMonitor monitor;
        monitor.logout();
        QCOMPARE(requests, 1);
        monitor.logout();
        QTest::qWait(100);
        QCOMPARE(requests, 1);
    }
};
QTEST_GUILESS_MAIN(LoginSessionMonitorTest)
#include "LoginSessionMonitorTest.moc"
