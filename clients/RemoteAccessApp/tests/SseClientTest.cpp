#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include "Network/Http/SseClient.h"
#include "Network/Http/ApiClient.h"

class SseClientTest : public QObject {
    Q_OBJECT
private slots:
    void oversizedIncompleteEventClosesConnection() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        ApiClient::instance().setBaseUrl(QStringLiteral("http://127.0.0.1:") + QString::number(server.serverPort()));
        SseClient client(QStringLiteral("/events"));
        QSignalSpy disconnected(&client, &SseClient::disconnected);
        QSignalSpy events(&client, &SseClient::eventReceived);
        client.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable() > 0); peer->readAll();
        peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\ndata: ");
        peer->write(QByteArray(1024 * 1024 + 1, 'x'));
        QTRY_COMPARE(disconnected.count(), 1);
        QCOMPARE(events.count(), 0);
        QVERIFY(client.isRunning());
        client.stop();
    }
    void transientFailureReconnectsButAuthenticationFailureStops() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        ApiClient::instance().setBaseUrl(QStringLiteral("http://127.0.0.1:") + QString::number(server.serverPort()));
        SseClient client(QStringLiteral("/events"));
        client.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable() > 0); peer->readAll();
        peer->write("HTTP/1.1 503 Unavailable\r\nContent-Length: 4\r\n\r\noops");
        peer->disconnectFromHost();
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 6000);
        QVERIFY(client.isRunning());
        peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable() > 0); peer->readAll();
        peer->write("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n");
        peer->disconnectFromHost();
        QTRY_VERIFY(!client.isRunning());
    }
    void deliversEventsAndStopsWithoutReentrantFinished() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        ApiClient::instance().setBaseUrl(QStringLiteral("http://127.0.0.1:") + QString::number(server.serverPort()));
        SseClient client(QStringLiteral("/events"));
        QSignalSpy events(&client, &SseClient::eventReceived);
        QSignalSpy connected(&client, &SseClient::connected);
        client.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection(); QVERIFY(peer);
        QTRY_VERIFY(peer->bytesAvailable() > 0); peer->readAll();
        peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\nevent: device-list\ndata: []\n\n");
        QTRY_COMPARE(connected.count(), 1);
        QTRY_COMPARE(events.count(), 1);
        QCOMPARE(events.at(0).at(0).toString(), QStringLiteral("device-list"));
        QCOMPARE(events.at(0).at(1).toByteArray(), QByteArray("[]"));
        client.stop();
        QVERIFY(!client.isRunning());
        QTRY_COMPARE(peer->state(), QAbstractSocket::UnconnectedState);
        client.stop();

    }
    void stopDuringConnectedSignalIsSafe() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        ApiClient::instance().setBaseUrl(QStringLiteral("http://127.0.0.1:") + QString::number(server.serverPort()));
        SseClient client(QStringLiteral("/events"));
        connect(&client, &SseClient::connected, &client, &SseClient::stop);
        client.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection(); QVERIFY(peer);
        QTRY_VERIFY(peer->bytesAvailable() > 0); peer->readAll();
        peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\ndata: test\n\n");
        QTRY_VERIFY(!client.isRunning());

    }
};
QTEST_GUILESS_MAIN(SseClientTest)
#include "SseClientTest.moc"
