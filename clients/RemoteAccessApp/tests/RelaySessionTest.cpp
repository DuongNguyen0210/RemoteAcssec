#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include "Network/Relay/ChildSessionController.h"
#include "Network/Relay/AdminSessionController.h"
#include "Network/Relay/StaticRelayEndpointProvider.h"
#include "Network/Relay/DynamicRelayEndpointProvider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include "Network/Http/ApiClient.h"
#include "Network/Protocol/ProtocolSerializer.h"

class RelaySessionTest : public QObject {
    Q_OBJECT
    QTcpServer m_api;
    QStringList m_paths;
    qint64 m_expiresAt = 0;
    static QByteArray packet(Protocol::MessageType type, quint64 id, QByteArray payload = {}, quint32 sequence = 0) {
        Protocol::ProtocolHeader header(type);
        header.sessionId = id;
        header.sequenceNumber = sequence;
        header.payloadLength = payload.size();
        return Protocol::ProtocolSerializer::serializeHeader(header) + payload;
    }
private slots:
    void initTestCase() {
        QVERIFY(m_api.listen(QHostAddress::LocalHost, 0));
        qputenv("REMOTE_API_URL", QByteArray("http://127.0.0.1:") + QByteArray::number(m_api.serverPort()));
        connect(&m_api, &QTcpServer::newConnection, this, [this] {
            auto *socket = m_api.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                buffer->append(socket->readAll());
                if (!buffer->contains("\r\n\r\n")) return;
                m_paths.append(QString::fromUtf8(buffer->left(buffer->indexOf("\r\n"))));
                const auto body = QJsonDocument(QJsonObject{{"success", true}, {"data", QJsonObject{
                    {"instanceId", "node"}, {"host", "127.0.0.1"}, {"port", 9092}, {"expiresAt", m_expiresAt}}}}).toJson();
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                              + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
    }

    void dynamicProviderAllocatesLooksUpAndRejectsExpiredEndpoints() {
        DynamicRelayEndpointProvider provider;
        m_expiresAt = QDateTime::currentMSecsSinceEpoch() + 30000;
        int count = 0;
        RelayEndpoint result;
        QString error;
        auto callback = [&](const RelayEndpoint &endpoint, const QString &reason) {
            result = endpoint; error = reason; ++count;
        };
        provider.allocate(callback);
        QTRY_COMPARE(count, 1);
        QVERIFY(error.isEmpty());
        QCOMPARE(result.port, quint16(9092));
        QCOMPARE(m_paths.last(), QString("POST /api/v1/relay/allocations HTTP/1.1"));
        const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        provider.lookup(id, callback);
        QTRY_COMPARE(count, 2);
        QCOMPARE(m_paths.last(), "GET /api/v1/devices/" + id + "/relay HTTP/1.1");
        m_expiresAt = QDateTime::currentMSecsSinceEpoch() - 1;
        provider.allocate(callback);
        QTRY_COMPARE(count, 3);
        QVERIFY(!error.isEmpty());
        QVERIFY(!result.isValid());
        provider.lookup("invalid", callback);
        QCOMPARE(count, 4);
        QVERIFY(!error.isEmpty());
    }

    void childRegistersNegotiatesForwardsInputAndResetsOnDisconnect() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        StaticRelayEndpointProvider provider("127.0.0.1", server.serverPort());
        ChildSessionController child(&provider);
        ApiClient::instance().setToken("test-token");
        QSignalSpy started(&child, &ChildSessionController::sessionStarted);
        QSignalSpy ended(&child, &ChildSessionController::sessionEnded);
        QByteArray inputPayload;
        connect(&child, &ChildSessionController::inputReceived, this,
                [&inputPayload](const auto &, const QByteArray &payload) { inputPayload = payload; });
        child.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        auto registration = peer->readAll();
        QCOMPARE(Protocol::ProtocolSerializer::deserializeHeader(registration.left(24))->type,
                 Protocol::MessageType::REGISTER_HOST);
        peer->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        peer->write(packet(Protocol::MessageType::SESSION_REQUEST, 7));
        QTRY_COMPARE(started.count(), 1);
        QCOMPARE(child.activeSessionId(), quint64(7));
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        QCOMPARE(peer->readAll(), packet(Protocol::MessageType::SESSION_ACCEPT, 7));
        QByteArray frame = packet(Protocol::MessageType::SCREEN_FRAME, 7, QByteArray::fromHex("00ffabcd"), 99);
        QCOMPARE(child.sendScreenPacket(frame), qint64(frame.size()));
        QTRY_COMPARE(peer->bytesAvailable(), qint64(frame.size()));
        QCOMPARE(peer->readAll(), frame);
        QCOMPARE(child.sendScreenPacket(packet(Protocol::MessageType::SCREEN_FRAME, 8, "wrong")), qint64(-1));
        peer->write(packet(Protocol::MessageType::KEY_PRESS, 8, "wrong"));
        peer->write(packet(Protocol::MessageType::MOUSE_MOVE, 7, "coordinates"));
        QTRY_COMPARE(inputPayload, QByteArray("coordinates"));
        peer->disconnectFromHost();
        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(child.activeSessionId(), quint64(0));
        child.stop();
        QCOMPARE(child.sendScreenPacket(frame), qint64(-1));
    }

    void adminUsesRequestedEndpointAndRoutesOnlyItsSession() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        AdminSessionController admin;
        ApiClient::instance().setToken("test-token");
        QSignalSpy established(&admin, &AdminSessionController::sessionEstablished);
        QSignalSpy failed(&admin, &AdminSessionController::sessionFailed);
        QSignalSpy ended(&admin, &AdminSessionController::sessionEnded);
        QByteArray screen;
        connect(&admin, &AdminSessionController::screenReceived, this,
                [&screen](const auto &, const QByteArray &payload) { screen = payload; });
        admin.requestSession(QUuid::createUuid().toString(QUuid::WithoutBraces), "127.0.0.1", server.serverPort());
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        auto request = peer->readAll();
        QCOMPARE(Protocol::ProtocolSerializer::deserializeHeader(request.left(24))->type,
                 Protocol::MessageType::CONNECT_REQUEST);
        peer->write(packet(Protocol::MessageType::CONNECT_RESULT, 9, QByteArray(1, '\1')));
        QTRY_COMPARE(established.count(), 1);
        QVERIFY(admin.sendInput(Protocol::MessageType::KEY_PRESS, "key"));
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        QCOMPARE(peer->readAll(), packet(Protocol::MessageType::KEY_PRESS, 9, "key"));
        QVERIFY(!admin.sendInput(Protocol::MessageType::SCREEN_FRAME, "invalid"));
        peer->write(packet(Protocol::MessageType::SCREEN_FRAME, 10, "wrong"));
        peer->write(packet(Protocol::MessageType::SCREEN_FRAME, 9, "screen"));
        QTRY_COMPARE(screen, QByteArray("screen"));
        admin.endSession();
        QCOMPARE(ended.count(), 1);
        QCOMPARE(failed.count(), 0);
        QVERIFY(!admin.sendInput(Protocol::MessageType::KEY_PRESS, "key"));
    }
};
QTEST_GUILESS_MAIN(RelaySessionTest)
#include "RelaySessionTest.moc"
