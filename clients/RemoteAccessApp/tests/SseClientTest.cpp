#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include "Network/Http/SseClient.h"

class SseClientTest : public QObject {
    Q_OBJECT
private slots:
    void deliversEventsAndStopsWithoutReentrantFinished() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        qputenv("REMOTE_API_URL", QByteArray("http://127.0.0.1:") + QByteArray::number(server.serverPort()));
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
        qunsetenv("REMOTE_API_URL");
    }
    void stopDuringConnectedSignalIsSafe() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        qputenv("REMOTE_API_URL", QByteArray("http://127.0.0.1:") + QByteArray::number(server.serverPort()));
        SseClient client(QStringLiteral("/events"));
        connect(&client, &SseClient::connected, &client, &SseClient::stop);
        client.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection(); QVERIFY(peer);
        QTRY_VERIFY(peer->bytesAvailable() > 0); peer->readAll();
        peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\ndata: test\n\n");
        QTRY_VERIFY(!client.isRunning());
        qunsetenv("REMOTE_API_URL");
    }
};
QTEST_GUILESS_MAIN(SseClientTest)
#include "SseClientTest.moc"
