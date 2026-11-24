#include <QtTest>
#include <QBuffer>
#include <QDataStream>
#include <QTcpServer>
#include <QTcpSocket>
#include "Network/Relay/ChildSessionController.h"
#include "Network/Relay/AdminSessionController.h"
#include "Network/Relay/RelayClient.h"
#include "Network/Relay/RelayEndpointProvider.h"
#include "Network/Relay/StaticRelayEndpointProvider.h"
#include "Network/Relay/DynamicRelayEndpointProvider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include "Network/Http/ApiClient.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Protocol/RelayAuthPayload.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include "Network/Protocol/ScreenFramePacketizer.h"
#include <utility>

class CallbackRelayEndpointProvider final : public RelayEndpointProvider {
public:
    explicit CallbackRelayEndpointProvider(QObject *parent = nullptr) : RelayEndpointProvider(parent) {}
    std::function<void(Callback)> onAllocate;

    void allocate(Callback callback) override { onAllocate(std::move(callback)); }
    void lookup(const QString &, Callback callback) override {
        callback({}, QStringLiteral("lookup không dùng trong test này."));
    }
};

class RelaySessionTest : public QObject {
    Q_OBJECT
    QTcpServer m_api;
    QStringList m_paths;
    qint64 m_expiresAt = 0;
    static QByteArray packet(Protocol::MessageType type, quint64 id, QByteArray payload = {},
                             quint32 sequence = 0, quint16 flags = 0) {
        Protocol::ProtocolHeader header(type);
        header.sessionId = id;
        header.sequenceNumber = sequence;
        header.flags = flags;
        header.payloadLength = payload.size();
        return Protocol::ProtocolSerializer::serializeHeader(header) + payload;
    }
    static QByteArray screenAck(quint64 sessionId, quint32 frameId, quint16 flags = 0) {
        return packet(static_cast<Protocol::MessageType>(0x11), sessionId, {}, frameId, flags);
    }

    static QByteArray childRegistration() {
        return screenAck(0, 0, 1) + packet(Protocol::MessageType::REGISTER_HOST, 0,
                                           Protocol::relayAuthPayload(ApiClient::instance().getToken()));
    }

    static QByteArray adminRequest(const QString &token, const QString &agent, bool firstRequest = true) {
        const auto request = packet(Protocol::MessageType::CONNECT_REQUEST, 0,
                                    Protocol::relayAuthPayload(token, agent));
        return firstRequest ? screenAck(0, 0, 1) + request : request;
    }
private slots:
    void tlsDoesNotExposeProtocolBeforeHandshake() {
        const QByteArray oldMode = qgetenv("REMOTE_RELAY_TLS");
        const QByteArray oldCa = qgetenv("REMOTE_RELAY_CA_CERT");
        qputenv("REMOTE_RELAY_TLS", "required");
        qunsetenv("REMOTE_RELAY_CA_CERT");
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RelayClient client;
        QSignalSpy ready(&client, &RelayClient::connected);
        client.ConnectToServer(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable() > 0);
        QCOMPARE(ready.count(), 0);
        QCOMPARE(client.sendRawPacket(QByteArray("secret-token")), qint64(-1));
        QVERIFY(!peer->readAll().contains("secret-token"));
        client.DisconnectFromServer();
        if (oldMode.isNull()) qunsetenv("REMOTE_RELAY_TLS"); else qputenv("REMOTE_RELAY_TLS", oldMode);
        if (oldCa.isNull()) qunsetenv("REMOTE_RELAY_CA_CERT"); else qputenv("REMOTE_RELAY_CA_CERT", oldCa);
    }

    void tlsCaConfigurationCannotFallBackToPlaintext() {
        const QByteArray oldMode = qgetenv("REMOTE_RELAY_TLS");
        const QByteArray oldCa = qgetenv("REMOTE_RELAY_CA_CERT");
        qputenv("REMOTE_RELAY_TLS", "off");
        qputenv("REMOTE_RELAY_CA_CERT", "/tmp/relay-ca.pem");
        RelayClient client;
        QSignalSpy error(&client, &RelayClient::transportError);
        client.ConnectToServer(QStringLiteral("localhost"), 1);
        QCOMPARE(error.count(), 1);
        QCOMPARE(client.sendRawPacket(QByteArray("secret-token")), qint64(-1));
        if (oldMode.isNull()) qunsetenv("REMOTE_RELAY_TLS"); else qputenv("REMOTE_RELAY_TLS", oldMode);
        if (oldCa.isNull()) qunsetenv("REMOTE_RELAY_CA_CERT"); else qputenv("REMOTE_RELAY_CA_CERT", oldCa);
    }

    void tlsConnectsOnlyToTrustedName() {
        const QByteArray portText = qgetenv("TEST_TLS_RELAY_PORT");
        const QByteArray certPath = qgetenv("TEST_TLS_RELAY_CERT");
        if (portText.isEmpty() || certPath.isEmpty()) QSKIP("Set TEST_TLS_RELAY_PORT and TEST_TLS_RELAY_CERT for TLS integration test");
        const QByteArray oldMode = qgetenv("REMOTE_RELAY_TLS");
        const QByteArray oldCa = qgetenv("REMOTE_RELAY_CA_CERT");
        qunsetenv("REMOTE_RELAY_TLS"); // A configured CA enables TLS even on loopback.
        qputenv("REMOTE_RELAY_CA_CERT", certPath);
        const quint16 port = portText.toUShort();
        QVERIFY(port != 0);
        {
            RelayClient client;
            QSignalSpy ready(&client, &RelayClient::connected);
            client.ConnectToServer(QStringLiteral("localhost"), port);
            QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 5000);
            QCOMPARE(client.sendRawPacket(QByteArray("tls-test")), qint64(8));
            client.DisconnectFromServer();
        }
        {
            RelayClient client;
            QSignalSpy ready(&client, &RelayClient::connected);
            QSignalSpy error(&client, &RelayClient::transportError);
            client.ConnectToServer(QStringLiteral("127.0.0.1"), port);
            QTRY_VERIFY_WITH_TIMEOUT(!error.isEmpty(), 5000);
            QCOMPARE(ready.count(), 0);
            QCOMPARE(client.sendRawPacket(QByteArray("secret-token")), qint64(-1));
            client.DisconnectFromServer();
        }
        if (oldMode.isNull()) qunsetenv("REMOTE_RELAY_TLS"); else qputenv("REMOTE_RELAY_TLS", oldMode);
        if (oldCa.isNull()) qunsetenv("REMOTE_RELAY_CA_CERT"); else qputenv("REMOTE_RELAY_CA_CERT", oldCa);
    }

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
        QSignalSpy registered(&child, &ChildSessionController::relayRegistered);
        QSignalSpy ended(&child, &ChildSessionController::sessionEnded);
        QSignalSpy failed(&child, &ChildSessionController::sessionFailed);
        QByteArray inputPayload;
        connect(&child, &ChildSessionController::inputReceived, this,
                [&inputPayload](const auto &, const QByteArray &payload) { inputPayload = payload; });
        child.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        const auto registration = childRegistration();
        QTRY_VERIFY(peer->bytesAvailable() >= registration.size());
        QCOMPARE(peer->readAll(), registration);
        peer->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        peer->write(packet(Protocol::MessageType::SESSION_REQUEST, 7));
        QTRY_COMPARE(registered.count(), 1);
        QVERIFY(child.relayReady());
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
        QVERIFY(!ended.at(0).at(0).toString().isEmpty());
        QCOMPARE(failed.count(), 0);
        QCOMPARE(child.activeSessionId(), quint64(0));
        child.stop();
        QCOMPARE(child.sendScreenPacket(frame), qint64(-1));
    }

    void childKeepsOneFrameInFlightWhileReceivingInput() {
        using T = Protocol::MessageType;
        constexpr quint64 sessionId = 71;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        StaticRelayEndpointProvider provider("127.0.0.1", server.serverPort());
        ChildSessionController child(&provider);
        ApiClient::instance().setToken("test-token");
        QByteArray input;
        connect(&child, &ChildSessionController::inputReceived, this,
                [&input](const auto &, const QByteArray &payload) { input = payload; });

        child.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        const auto registration = childRegistration();
        QTRY_COMPARE(peer->bytesAvailable(), qint64(registration.size()));
        QCOMPARE(peer->readAll(), registration);
        peer->write(packet(T::REGISTER_ACK, 0, QByteArray(1, '\1'))
                    + screenAck(sessionId, 0, 1) + packet(T::SESSION_REQUEST, sessionId));
        QTRY_VERIFY(child.canSendScreenFrame());
        const auto accept = packet(T::SESSION_ACCEPT, sessionId);
        QTRY_COMPARE(peer->bytesAvailable(), qint64(accept.size()));
        QCOMPARE(peer->readAll(), accept);

        const auto first = ScreenFramePacketizer::packetize("first frame", 1, sessionId);
        QVERIFY(child.sendScreenFrame(1, first));
        QTRY_COMPARE(peer->bytesAvailable(), qint64(first.constFirst().size()));
        QCOMPARE(peer->readAll(), first.constFirst());
        QTRY_COMPARE(child.pendingBytes(), qint64(0));
        const auto stale = ScreenFramePacketizer::packetize("stale frame", 2, sessionId);
        QVERIFY(!child.sendScreenFrame(2, stale));

        const auto key = RemoteInputPacket::key(Qt::Key_A);
        peer->write(screenAck(sessionId + 1, 1) + screenAck(sessionId, 2)
                    + packet(T::KEY_PRESS, sessionId, key));
        QTRY_COMPARE_WITH_TIMEOUT(input, key, 1500);
        QVERIFY(!child.canSendScreenFrame());
        QCOMPARE(peer->bytesAvailable(), qint64(0));

        const auto ack = screenAck(sessionId, 1);
        QSignalSpy received(child.relayClient(), &RelayClient::bytesReceived);
        peer->write(ack.left(11));
        QTRY_VERIFY(!received.isEmpty());
        QVERIFY(!child.canSendScreenFrame());
        peer->write(ack.mid(11));
        QTRY_VERIFY(child.canSendScreenFrame());
        const auto latest = ScreenFramePacketizer::packetize("latest frame", 3, sessionId);
        QVERIFY(child.sendScreenFrame(3, latest));
        QTRY_COMPARE(peer->bytesAvailable(), qint64(latest.constFirst().size()));
        QCOMPARE(peer->readAll(), latest.constFirst());
        child.stop();
    }

    void adminAcknowledgesOnlyCompletedNegotiatedFrames_data() {
        QTest::addColumn<bool>("acknowledgmentsEnabled");
        QTest::newRow("legacy relay") << false;
        QTest::newRow("negotiated relay") << true;
    }

    void adminAcknowledgesOnlyCompletedNegotiatedFrames() {
        QFETCH(bool, acknowledgmentsEnabled);
        using T = Protocol::MessageType;
        constexpr quint64 sessionId = 42;
        constexpr quint32 frameId = 23;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        auto *provider = new StaticRelayEndpointProvider("127.0.0.1", server.serverPort());
        AdminSessionController admin(provider, nullptr);
        const QString token = "test-token";
        const QString agent = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
        ApiClient::instance().setToken(token);
        QSignalSpy ready(&admin, &AdminSessionController::frameReady);
        admin.requestSession(agent);
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        const auto request = adminRequest(token, agent);
        QTRY_COMPARE(peer->bytesAvailable(), qint64(request.size()));
        QCOMPARE(peer->readAll(), request);

        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(Qt::red);
        QByteArray jpeg;
        QBuffer buffer(&jpeg);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "JPEG"));
        const auto chunk = [&](quint32 index, const QByteArray &bytes) {
            QByteArray payload;
            QDataStream fields(&payload, QIODevice::WriteOnly);
            fields << frameId << index << quint32(2) << quint32(jpeg.size());
            return packet(T::SCREEN_FRAME, sessionId, payload + bytes, index);
        };
        const auto established = packet(T::CONNECT_RESULT, sessionId, QByteArray(1, '\1'));
        const auto enable = acknowledgmentsEnabled ? screenAck(sessionId, 0, 1) : QByteArray();
        QSignalSpy outbound(peer, &QTcpSocket::readyRead);
        peer->write(established + enable + chunk(0, jpeg.left(jpeg.size() / 2)));
        QVERIFY(!outbound.wait(350));
        QCOMPARE(ready.count(), 0);
        peer->write(chunk(1, jpeg.mid(jpeg.size() / 2)));
        QTRY_COMPARE(ready.count(), 1);
        QCOMPARE(ready.at(0).at(0).toULongLong(), sessionId);
        QCOMPARE(qvariant_cast<QImage>(ready.at(0).at(1)).size(), image.size());
        if (acknowledgmentsEnabled) {
            const auto expectedAck = screenAck(sessionId, frameId);
            QTRY_COMPARE(peer->bytesAvailable(), qint64(expectedAck.size()));
            QCOMPARE(peer->readAll(), expectedAck);
        } else {
            QVERIFY(!outbound.wait(350));
            QCOMPARE(peer->bytesAvailable(), qint64(0));
        }
    }

    void childBacksOffRepeatedAllocationFailures() {
        CallbackRelayEndpointProvider provider;
        int allocations = 0;
        provider.onAllocate = [&](RelayEndpointProvider::Callback callback) {
            ++allocations;
            callback({}, QStringLiteral("Relay unavailable"));
        };
        ChildSessionController child(&provider);
        child.start();
        QCOMPARE(allocations, 1);
        QTRY_COMPARE_WITH_TIMEOUT(allocations, 2, 2500);
        QTest::qWait(2200);
        QCOMPARE(allocations, 2);
        QTRY_COMPARE_WITH_TIMEOUT(allocations, 3, 2000);
        child.stop();
    }

    void childPendingTimeoutCloseResetsAndRegistersOnceAfterReconnect() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        QSignalSpy connections(&server, &QTcpServer::newConnection);
        StaticRelayEndpointProvider provider("127.0.0.1", server.serverPort());
        ChildSessionController child(&provider);
        ApiClient::instance().setToken("test-token");
        QSignalSpy started(&child, &ChildSessionController::sessionStarted);
        QSignalSpy ended(&child, &ChildSessionController::sessionEnded);
        QSignalSpy failed(&child, &ChildSessionController::sessionFailed);

        child.start();
        QTRY_COMPARE(connections.count(), 1);
        auto *firstPeer = server.nextPendingConnection();
        QVERIFY(firstPeer != nullptr);
        const auto firstRegistration = childRegistration();
        QTRY_VERIFY(firstPeer->bytesAvailable() >= firstRegistration.size());
        QCOMPARE(firstPeer->readAll(), firstRegistration);
        firstPeer->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        firstPeer->write(packet(Protocol::MessageType::SESSION_REQUEST, 70));
        QTRY_COMPARE(started.count(), 1);
        QCOMPARE(child.activeSessionId(), quint64(70));
        QTRY_VERIFY(firstPeer->bytesAvailable() >= 24);
        QCOMPARE(firstPeer->readAll(), packet(Protocol::MessageType::SESSION_ACCEPT, 70));

        firstPeer->disconnectFromHost(); // Relay closes CHILD after pending timeout.
        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(child.activeSessionId(), quint64(0));
        QCOMPARE(failed.count(), 0);
        child.relayClient()->disconnected(); // A duplicate notification must not restart retry.
        QCOMPARE(ended.count(), 1);
        QCOMPARE(failed.count(), 0);

        QTRY_COMPARE_WITH_TIMEOUT(connections.count(), 2, 4000);
        auto *reconnectedPeer = server.nextPendingConnection();
        QVERIFY(reconnectedPeer != nullptr);
        const auto newRegistration = childRegistration();
        QTRY_VERIFY(reconnectedPeer->bytesAvailable() >= newRegistration.size());
        QCOMPARE(reconnectedPeer->readAll(), newRegistration);
        QVERIFY(!connections.wait(1700));
        QCOMPARE(connections.count(), 2);
        QCOMPARE(reconnectedPeer->bytesAvailable(), qint64(0));

        reconnectedPeer->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        reconnectedPeer->write(packet(Protocol::MessageType::SESSION_REQUEST, 71));
        QTRY_COMPARE(started.count(), 2);
        QCOMPARE(child.activeSessionId(), quint64(71));
        QTRY_VERIFY(reconnectedPeer->bytesAvailable() >= 24);
        QCOMPARE(reconnectedPeer->readAll(), packet(Protocol::MessageType::SESSION_ACCEPT, 71));
        QCOMPARE(ended.count(), 1);
        QCOMPARE(failed.count(), 0);
        child.stop();
        ApiClient::instance().setToken({});
    }

    void childReportsPreActiveParserFatalAndReconnects() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        StaticRelayEndpointProvider provider("127.0.0.1", server.serverPort());
        ChildSessionController child(&provider);
        ApiClient::instance().setToken("test-token");
        QSignalSpy started(&child, &ChildSessionController::sessionStarted);
        QSignalSpy ended(&child, &ChildSessionController::sessionEnded);
        QSignalSpy failed(&child, &ChildSessionController::sessionFailed);

        child.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QVERIFY(peer != nullptr);
        const auto registration = childRegistration();
        QTRY_VERIFY(peer->bytesAvailable() >= registration.size());
        QCOMPARE(peer->readAll(), registration);
        QCOMPARE(child.activeSessionId(), quint64(0));
        QCOMPARE(started.count(), 0);

        const QByteArray malformedHeader(24, '\0');
        QCOMPARE(peer->write(malformedHeader), qint64(malformedHeader.size()));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("Gói tin Relay không hợp lệ."));
        QCOMPARE(ended.count(), 0);
        QCOMPARE(child.activeSessionId(), quint64(0));
        QTRY_COMPARE(peer->state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(failed.count(), 1);

        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 4000);
        auto *reconnectedPeer = server.nextPendingConnection();
        QVERIFY(reconnectedPeer != nullptr);
        const auto newRegistration = childRegistration();
        QTRY_VERIFY(reconnectedPeer->bytesAvailable() >= newRegistration.size());
        QCOMPARE(reconnectedPeer->readAll(), newRegistration);
        QCOMPARE(started.count(), 0);
        QCOMPARE(child.activeSessionId(), quint64(0));
        QCOMPARE(failed.count(), 1);
        child.stop();
    }

    void childReportsActiveParserFatalAndStartsFreshSession() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        StaticRelayEndpointProvider provider("127.0.0.1", server.serverPort());
        ChildSessionController child(&provider);
        ApiClient::instance().setToken("test-token");
        QSignalSpy started(&child, &ChildSessionController::sessionStarted);
        QSignalSpy ended(&child, &ChildSessionController::sessionEnded);
        QSignalSpy failed(&child, &ChildSessionController::sessionFailed);

        child.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QVERIFY(peer != nullptr);
        const auto registration = childRegistration();
        QTRY_VERIFY(peer->bytesAvailable() >= registration.size());
        QCOMPARE(peer->readAll(), registration);
        peer->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        peer->write(packet(Protocol::MessageType::SESSION_REQUEST, 7));
        QTRY_COMPARE(started.count(), 1);
        QCOMPARE(child.activeSessionId(), quint64(7));
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        QCOMPARE(peer->readAll(), packet(Protocol::MessageType::SESSION_ACCEPT, 7));

        const QByteArray malformedHeader(24, '\0');
        QCOMPARE(peer->write(malformedHeader), qint64(malformedHeader.size()));
        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(ended.at(0).at(0).toString(), QStringLiteral("Gói tin Relay không hợp lệ."));
        QCOMPARE(child.activeSessionId(), quint64(0));
        QCOMPARE(failed.count(), 0);
        QTRY_COMPARE(peer->state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(ended.count(), 1);

        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 4000);
        auto *reconnectedPeer = server.nextPendingConnection();
        QVERIFY(reconnectedPeer != nullptr);
        const auto newRegistration = childRegistration();
        QTRY_VERIFY(reconnectedPeer->bytesAvailable() >= newRegistration.size());
        QCOMPARE(reconnectedPeer->readAll(), newRegistration);
        QCOMPARE(child.activeSessionId(), quint64(0));
        QCOMPARE(started.count(), 1);
        QCOMPARE(ended.count(), 1);

        reconnectedPeer->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        QCOMPARE(started.count(), 1);
        reconnectedPeer->write(packet(Protocol::MessageType::SESSION_REQUEST, 8));
        QTRY_COMPARE(started.count(), 2);
        QCOMPARE(child.activeSessionId(), quint64(8));
        QTRY_VERIFY(reconnectedPeer->bytesAvailable() >= 24);
        QCOMPARE(reconnectedPeer->readAll(), packet(Protocol::MessageType::SESSION_ACCEPT, 8));
        QCOMPARE(failed.count(), 0);
        child.stop();
    }

    void childStopsActiveSessionWithoutReconnecting() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        StaticRelayEndpointProvider provider("127.0.0.1", server.serverPort());
        ChildSessionController child(&provider);
        ApiClient::instance().setToken("test-token");
        QSignalSpy started(&child, &ChildSessionController::sessionStarted);
        QSignalSpy ended(&child, &ChildSessionController::sessionEnded);
        QSignalSpy failed(&child, &ChildSessionController::sessionFailed);

        child.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QVERIFY(peer != nullptr);
        const auto registration = childRegistration();
        QTRY_VERIFY(peer->bytesAvailable() >= registration.size());
        QCOMPARE(peer->readAll(), registration);
        peer->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        peer->write(packet(Protocol::MessageType::SESSION_REQUEST, 42));
        QTRY_COMPARE(started.count(), 1);
        QCOMPARE(child.activeSessionId(), quint64(42));
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        QCOMPARE(peer->readAll(), packet(Protocol::MessageType::SESSION_ACCEPT, 42));

        QSignalSpy reconnections(&server, &QTcpServer::newConnection);
        child.stop();
        QCOMPARE(ended.count(), 1);
        QCOMPARE(ended.at(0).at(0).toString(), QStringLiteral("Đã dừng kết nối Relay."));
        QCOMPARE(child.activeSessionId(), quint64(0));
        QTRY_COMPARE(peer->state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(failed.count(), 0);
        QVERIFY(!reconnections.wait(1800));
        QCOMPARE(reconnections.count(), 0);
        QVERIFY(!server.hasPendingConnections());
    }

    void childChangesRelayEndpointAfterActiveSession() {
        QTcpServer serverA;
        QTcpServer serverB;
        QVERIFY(serverA.listen(QHostAddress::LocalHost, 0));
        QVERIFY(serverB.listen(QHostAddress::LocalHost, 0));
        const qint64 expiry = QDateTime::currentMSecsSinceEpoch() + 60000;
        const RelayEndpoint endpointA{QStringLiteral("relay-a"), QStringLiteral("127.0.0.1"), serverA.serverPort(), expiry};
        const RelayEndpoint endpointB{QStringLiteral("relay-b"), QStringLiteral("127.0.0.1"), serverB.serverPort(), expiry};
        CallbackRelayEndpointProvider provider;
        int allocations = 0;
        provider.onAllocate = [&](RelayEndpointProvider::Callback callback) {
            callback(++allocations == 1 ? endpointA : endpointB, {});
        };
        ChildSessionController child(&provider);
        ApiClient::instance().setToken("test-token");
        QSignalSpy started(&child, &ChildSessionController::sessionStarted);
        QSignalSpy ended(&child, &ChildSessionController::sessionEnded);
        QSignalSpy failed(&child, &ChildSessionController::sessionFailed);

        child.start();
        QTRY_VERIFY(serverA.hasPendingConnections());
        auto *peerA = serverA.nextPendingConnection();
        QVERIFY(peerA != nullptr);
        const auto registrationA = childRegistration();
        QTRY_VERIFY(peerA->bytesAvailable() >= registrationA.size());
        QCOMPARE(peerA->readAll(), registrationA);
        peerA->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        peerA->write(packet(Protocol::MessageType::SESSION_REQUEST, 42));
        QTRY_COMPARE(started.count(), 1);
        QCOMPARE(child.activeSessionId(), quint64(42));
        QTRY_VERIFY(peerA->bytesAvailable() >= 24);
        QCOMPARE(peerA->readAll(), packet(Protocol::MessageType::SESSION_ACCEPT, 42));

        quint64 sessionIdAtEnd = 42;
        auto oldPeerStateAtEnd = QAbstractSocket::UnconnectedState;
        connect(&child, &ChildSessionController::sessionEnded, this, [&](const QString &) {
            sessionIdAtEnd = child.activeSessionId();
            oldPeerStateAtEnd = peerA->state();
        });
        QTRY_VERIFY_WITH_TIMEOUT(serverB.hasPendingConnections(), 16000);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(ended.at(0).at(0).toString(), QStringLiteral("Endpoint Relay đã thay đổi."));
        QCOMPARE(sessionIdAtEnd, quint64(0));
        QCOMPARE(oldPeerStateAtEnd, QAbstractSocket::ConnectedState);
        QCOMPARE(child.activeSessionId(), quint64(0));
        QTRY_COMPARE(peerA->state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(failed.count(), 0);

        auto *peerB = serverB.nextPendingConnection();
        QVERIFY(peerB != nullptr);
        const auto registrationB = childRegistration();
        QTRY_VERIFY(peerB->bytesAvailable() >= registrationB.size());
        QCOMPARE(peerB->readAll(), registrationB);
        child.stop();
    }

    void childDoesNotRestartPendingReconnect() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        StaticRelayEndpointProvider provider("127.0.0.1", server.serverPort());
        ChildSessionController child(&provider);
        ApiClient::instance().setToken("test-token");
        QSignalSpy failed(&child, &ChildSessionController::sessionFailed);

        child.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QVERIFY(peer != nullptr);
        const auto registration = childRegistration();
        QTRY_VERIFY(peer->bytesAvailable() >= registration.size());
        QCOMPARE(peer->readAll(), registration);
        const QByteArray malformedHeader(24, '\0');
        QCOMPARE(peer->write(malformedHeader), qint64(malformedHeader.size()));
        QTRY_COMPARE(failed.count(), 1);
        QTRY_COMPARE(peer->state(), QAbstractSocket::UnconnectedState);

        bool duplicateDelivered = false;
        QTimer::singleShot(900, &child, [&] {
            duplicateDelivered = true;
            child.relayClient()->disconnected();
        });
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 2100);
        QVERIFY(duplicateDelivered);
        auto *reconnectedPeer = server.nextPendingConnection();
        QVERIFY(reconnectedPeer != nullptr);
        const auto newRegistration = childRegistration();
        QTRY_VERIFY(reconnectedPeer->bytesAvailable() >= newRegistration.size());
        QCOMPARE(reconnectedPeer->readAll(), newRegistration);
        child.stop();
    }

    void childIgnoresStaleAllocationCallback() {
        QTcpServer oldServer;
        QTcpServer currentServer;
        QVERIFY(oldServer.listen(QHostAddress::LocalHost, 0));
        QVERIFY(currentServer.listen(QHostAddress::LocalHost, 0));
        const RelayEndpoint oldEndpoint{QStringLiteral("old"), QStringLiteral("127.0.0.1"), oldServer.serverPort(), 0};
        const RelayEndpoint currentEndpoint{QStringLiteral("current"), QStringLiteral("127.0.0.1"), currentServer.serverPort(), 0};
        CallbackRelayEndpointProvider provider;
        QList<RelayEndpointProvider::Callback> pending;
        provider.onAllocate = [&](RelayEndpointProvider::Callback callback) {
            pending.append(std::move(callback));
        };
        ChildSessionController child(&provider);
        ApiClient::instance().setToken("test-token");
        QSignalSpy started(&child, &ChildSessionController::sessionStarted);
        QSignalSpy ended(&child, &ChildSessionController::sessionEnded);

        child.start();
        QCOMPARE(pending.size(), 1);
        child.stop();
        child.start();
        QCOMPARE(pending.size(), 2);
        pending.at(1)(currentEndpoint, {});
        QTRY_VERIFY(currentServer.hasPendingConnections());
        auto *currentPeer = currentServer.nextPendingConnection();
        QVERIFY(currentPeer != nullptr);
        const auto registration = childRegistration();
        QTRY_VERIFY(currentPeer->bytesAvailable() >= registration.size());
        QCOMPARE(currentPeer->readAll(), registration);
        currentPeer->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        currentPeer->write(packet(Protocol::MessageType::SESSION_REQUEST, 84));
        QTRY_COMPARE(started.count(), 1);
        QCOMPARE(child.activeSessionId(), quint64(84));
        QTRY_VERIFY(currentPeer->bytesAvailable() >= 24);
        QCOMPARE(currentPeer->readAll(), packet(Protocol::MessageType::SESSION_ACCEPT, 84));

        QSignalSpy wrongConnections(&oldServer, &QTcpServer::newConnection);
        pending.at(0)(oldEndpoint, {});
        QCOMPARE(child.activeSessionId(), quint64(84));
        QCOMPARE(ended.count(), 0);
        QVERIFY(!wrongConnections.wait(200));
        QCOMPARE(wrongConnections.count(), 0);
        QCOMPARE(currentPeer->state(), QAbstractSocket::ConnectedState);
        const QByteArray frame = packet(Protocol::MessageType::SCREEN_FRAME, 84, QByteArray("new-session"));
        QCOMPARE(child.sendScreenPacket(frame), qint64(frame.size()));
        QTRY_COMPARE(currentPeer->bytesAvailable(), qint64(frame.size()));
        QCOMPARE(currentPeer->readAll(), frame);
        child.stop();
    }

    void childPreservesActiveSessionOnTransientRenewalFailureWhileLeaseValid() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        const qint64 expiry = QDateTime::currentMSecsSinceEpoch() + 45000;
        const RelayEndpoint validEndpoint{QStringLiteral("relay-1"), QStringLiteral("127.0.0.1"), server.serverPort(), expiry};
        CallbackRelayEndpointProvider provider;
        int allocations = 0;
        provider.onAllocate = [&](RelayEndpointProvider::Callback callback) {
            if (++allocations == 1) {
                callback(validEndpoint, {});
            } else {
                callback({}, QStringLiteral("Kết nối quá hạn. Thử lại."));
            }
        };
        ChildSessionController child(&provider);
        ApiClient::instance().setToken("test-token");
        QSignalSpy started(&child, &ChildSessionController::sessionStarted);
        QSignalSpy ended(&child, &ChildSessionController::sessionEnded);
        QSignalSpy failed(&child, &ChildSessionController::sessionFailed);

        child.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QVERIFY(peer != nullptr);
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        peer->readAll();
        peer->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        peer->write(packet(Protocol::MessageType::SESSION_REQUEST, 99));
        QTRY_COMPARE(started.count(), 1);
        QCOMPARE(child.activeSessionId(), quint64(99));
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        peer->readAll();

        // Wait for the 10s renewal tick to fire and return a transient error while the 45s lease is still valid
        QTRY_COMPARE_WITH_TIMEOUT(allocations, 2, 12000);
        QCOMPARE(child.activeSessionId(), quint64(99));
        QCOMPARE(ended.count(), 0);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(peer->state(), QAbstractSocket::ConnectedState);
        child.stop();
    }

    void adminUsesConfiguredEndpointAndReportsActiveDisconnect() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        const QString agentSessionId = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
        const QString token = QStringLiteral("test-token");
        qputenv("REMOTE_RELAY_HOST", "127.0.0.1");
        qputenv("REMOTE_RELAY_PORT", QByteArray::number(server.serverPort()));
        AdminSessionController admin;
        ApiClient::instance().setToken(token);
        QSignalSpy established(&admin, &AdminSessionController::sessionEstablished);
        QSignalSpy failed(&admin, &AdminSessionController::sessionFailed);
        QSignalSpy requestFailed(&admin, &AdminSessionController::requestFailed);
        QSignalSpy ended(&admin, &AdminSessionController::sessionEnded);
        QVERIFY(established.isValid());
        QVERIFY(failed.isValid());
        QVERIFY(requestFailed.isValid());
        QVERIFY(ended.isValid());

        admin.requestSession(agentSessionId);
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QVERIFY(peer != nullptr);

        const QByteArray expectedRequest = adminRequest(token, agentSessionId);
        QTRY_VERIFY(peer->bytesAvailable() >= expectedRequest.size());
        QCOMPARE(peer->read(expectedRequest.size()), expectedRequest);

        peer->write(packet(Protocol::MessageType::CONNECT_RESULT, 9, QByteArray(1, '\1')));
        QTRY_COMPARE(established.count(), 1);
        QCOMPARE(established.at(0).at(0).toULongLong(), quint64(9));
        QCOMPARE(established.at(0).at(1).toString(), agentSessionId);
        QCOMPARE(requestFailed.count(), 0);

        peer->disconnectFromHost();
        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(ended.at(0).at(0).toULongLong(), quint64(9));
        QCOMPARE(ended.at(0).at(1).toString(), agentSessionId);
        QVERIFY(!ended.at(0).at(2).toString().isEmpty());

        QCOMPARE(failed.count(), 0);
        QCOMPARE(requestFailed.count(), 0);

        ApiClient::instance().setToken({});
        qunsetenv("REMOTE_RELAY_HOST");
        qunsetenv("REMOTE_RELAY_PORT");
    }

    void adminPendingTimeoutFailureAllowsRetryWithoutDuplicateSignals() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        const QString agentA = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
        const QString agentB = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb");
        const QString token = QStringLiteral("test-token");
        qputenv("REMOTE_RELAY_HOST", "127.0.0.1");
        qputenv("REMOTE_RELAY_PORT", QByteArray::number(server.serverPort()));
        ApiClient::instance().setToken(token);
        AdminSessionController admin;
        QSignalSpy requestFailed(&admin, &AdminSessionController::requestFailed);
        QSignalSpy established(&admin, &AdminSessionController::sessionEstablished);
        QSignalSpy ended(&admin, &AdminSessionController::sessionEnded);
        QSignalSpy failed(&admin, &AdminSessionController::sessionFailed);

        admin.requestSession(agentA);
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QVERIFY(peer != nullptr);
        const QByteArray firstRequest = adminRequest(token, agentA);
        QTRY_VERIFY(peer->bytesAvailable() >= firstRequest.size());
        QCOMPARE(peer->read(firstRequest.size()), firstRequest);
        QCOMPARE(requestFailed.count(), 0);

        const QByteArray timeoutFailure = packet(Protocol::MessageType::CONNECT_RESULT, 0, QByteArray(1, '\0'));
        QCOMPARE(peer->write(timeoutFailure + timeoutFailure), qint64(timeoutFailure.size() * 2));
        QVERIFY(peer->flush());
        QTRY_COMPARE(requestFailed.count(), 1);
        QTest::qWait(50);
        QCOMPARE(requestFailed.count(), 1);
        QCOMPARE(requestFailed.at(0).at(0).toString(), agentA);
        QVERIFY(!requestFailed.at(0).at(1).toString().isEmpty());
        QCOMPARE(established.count(), 0);
        QCOMPARE(ended.count(), 0);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(peer->state(), QAbstractSocket::ConnectedState);

        admin.requestSession(agentB);
        const QByteArray secondRequest = adminRequest(token, agentB, false);
        QTRY_VERIFY(peer->bytesAvailable() >= secondRequest.size());
        QCOMPARE(peer->read(secondRequest.size()), secondRequest);
        QCOMPARE(requestFailed.count(), 1);
        peer->write(packet(Protocol::MessageType::CONNECT_RESULT, 72, QByteArray(1, '\1')));
        QTRY_COMPARE(established.count(), 1);
        QCOMPARE(established.at(0).at(0).toULongLong(), quint64(72));
        QCOMPARE(established.at(0).at(1).toString(), agentB);
        QCOMPARE(requestFailed.count(), 1);
        QCOMPARE(ended.count(), 0);
        QCOMPARE(failed.count(), 0);

        ApiClient::instance().setToken({});
        qunsetenv("REMOTE_RELAY_HOST");
        qunsetenv("REMOTE_RELAY_PORT");
    }

    void adminGracefulDisconnectKeepsTransportAndIsIdempotent() {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        qputenv("REMOTE_RELAY_HOST", "127.0.0.1");
        qputenv("REMOTE_RELAY_PORT", QByteArray::number(server.serverPort()));
        ApiClient::instance().setToken("test-token");
        AdminSessionController admin;
        QSignalSpy established(&admin, &AdminSessionController::sessionEstablished);
        QSignalSpy ended(&admin, &AdminSessionController::sessionEnded);
        admin.requestSession(QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"));
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection(); QVERIFY(peer);
        QTRY_VERIFY(peer->bytesAvailable() >= 24); peer->readAll();
        peer->write(packet(Protocol::MessageType::CONNECT_RESULT, 42, QByteArray(1, '\1')));
        QTRY_COMPARE(established.count(), 1);
        admin.disconnectSession(); admin.disconnectSession();
        QCOMPARE(ended.count(), 1);
        QCOMPARE(ended.at(0).at(0).toULongLong(), quint64(42));
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        QCOMPARE(peer->readAll(), packet(Protocol::MessageType::DISCONNECT, 42));
        QCOMPARE(peer->state(), QAbstractSocket::ConnectedState);
        admin.requestSession(QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"));
        QTRY_VERIFY(peer->bytesAvailable() >= 24); peer->readAll();
        peer->write(packet(Protocol::MessageType::CONNECT_RESULT, 44, QByteArray(1, '\1')));
        QTRY_COMPARE(established.count(), 2);
        peer->write(packet(Protocol::MessageType::DISCONNECT, 42));
        QTest::qWait(50); QCOMPARE(ended.count(), 1);
        peer->disconnectFromHost(); QTRY_COMPARE(ended.count(), 2);
        ApiClient::instance().setToken({}); qunsetenv("REMOTE_RELAY_HOST"); qunsetenv("REMOTE_RELAY_PORT");
    }

    void childGracefulDisconnectStopsSessionAndAllowsAnother() {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        StaticRelayEndpointProvider provider("127.0.0.1", server.serverPort());
        ChildSessionController child(&provider); ApiClient::instance().setToken("test-token");
        QSignalSpy started(&child, &ChildSessionController::sessionStarted);
        QSignalSpy ended(&child, &ChildSessionController::sessionEnded);
        child.start(); QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection(); QVERIFY(peer);
        QTRY_VERIFY(peer->bytesAvailable() >= 24); peer->readAll();
        peer->write(packet(Protocol::MessageType::REGISTER_ACK, 0, QByteArray(1, '\1')));
        peer->write(packet(Protocol::MessageType::SESSION_REQUEST, 42));
        QTRY_COMPARE(started.count(), 1); QCOMPARE(child.activeSessionId(), quint64(42));
        QTRY_VERIFY(peer->bytesAvailable() >= 24); peer->readAll();
        peer->write(packet(Protocol::MessageType::DISCONNECT, 43));
        QTest::qWait(50); QCOMPARE(child.activeSessionId(), quint64(42));
        peer->write(packet(Protocol::MessageType::DISCONNECT, 42));
        QTRY_COMPARE(ended.count(), 1); QCOMPARE(child.activeSessionId(), quint64(0));
        QCOMPARE(child.sendScreenPacket(packet(Protocol::MessageType::SCREEN_FRAME, 42)), qint64(-1));
        peer->write(packet(Protocol::MessageType::DISCONNECT, 42));
        peer->write(packet(Protocol::MessageType::SESSION_REQUEST, 44));
        QTRY_COMPARE(started.count(), 2); QCOMPARE(child.activeSessionId(), quint64(44));
        QCOMPARE(ended.count(), 1); QCOMPARE(peer->state(), QAbstractSocket::ConnectedState);
        peer->disconnectFromHost(); QTRY_COMPARE(ended.count(), 2);
        ApiClient::instance().setToken({});
    }

    void adminReportsActiveSessionEndedOnParserFatalError() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        const QString agentSessionId = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
        const QString token = QStringLiteral("test-token");
        qputenv("REMOTE_RELAY_HOST", "127.0.0.1");
        qputenv("REMOTE_RELAY_PORT", QByteArray::number(server.serverPort()));
        AdminSessionController admin;
        ApiClient::instance().setToken(token);
        QSignalSpy established(&admin, &AdminSessionController::sessionEstablished);
        QSignalSpy failed(&admin, &AdminSessionController::sessionFailed);
        QSignalSpy requestFailed(&admin, &AdminSessionController::requestFailed);
        QSignalSpy ended(&admin, &AdminSessionController::sessionEnded);
        QVERIFY(established.isValid());
        QVERIFY(failed.isValid());
        QVERIFY(requestFailed.isValid());
        QVERIFY(ended.isValid());

        admin.requestSession(agentSessionId);
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer = server.nextPendingConnection();
        QVERIFY(peer != nullptr);

        const QByteArray expectedRequest = adminRequest(token, agentSessionId);
        QTRY_VERIFY(peer->bytesAvailable() >= expectedRequest.size());
        QCOMPARE(peer->read(expectedRequest.size()), expectedRequest);

        peer->write(packet(Protocol::MessageType::CONNECT_RESULT, 9, QByteArray(1, '\1')));
        QTRY_COMPARE(established.count(), 1);
        QCOMPARE(established.at(0).at(0).toULongLong(), quint64(9));
        QCOMPARE(established.at(0).at(1).toString(), agentSessionId);
        QCOMPARE(ended.count(), 0);

        const QByteArray malformedHeader(24, '\0');
        QCOMPARE(peer->write(malformedHeader), qint64(malformedHeader.size()));
        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(ended.at(0).at(0).toULongLong(), quint64(9));
        QCOMPARE(ended.at(0).at(1).toString(), agentSessionId);
        QVERIFY(!ended.at(0).at(2).toString().isEmpty());
        QTRY_COMPARE(peer->state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(requestFailed.count(), 0);

        ApiClient::instance().setToken({});
        qunsetenv("REMOTE_RELAY_HOST");
        qunsetenv("REMOTE_RELAY_PORT");
    }
};
QTEST_GUILESS_MAIN(RelaySessionTest)
#include "RelaySessionTest.moc"
