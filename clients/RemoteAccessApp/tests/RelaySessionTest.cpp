#include <QtTest>
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
        QSignalSpy failed(&child, &ChildSessionController::sessionFailed);
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
        QVERIFY(!ended.at(0).at(0).toString().isEmpty());
        QCOMPARE(failed.count(), 0);
        QCOMPARE(child.activeSessionId(), quint64(0));
        child.stop();
        QCOMPARE(child.sendScreenPacket(frame), qint64(-1));
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
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        const auto registration = Protocol::ProtocolSerializer::deserializeHeader(peer->readAll().left(24));
        QVERIFY(registration.has_value());
        QCOMPARE(registration->type, Protocol::MessageType::REGISTER_HOST);
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
        QTRY_VERIFY(reconnectedPeer->bytesAvailable() >= 24);
        const auto newRegistration = Protocol::ProtocolSerializer::deserializeHeader(reconnectedPeer->readAll().left(24));
        QVERIFY(newRegistration.has_value());
        QCOMPARE(newRegistration->type, Protocol::MessageType::REGISTER_HOST);
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
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        const auto registration = Protocol::ProtocolSerializer::deserializeHeader(peer->readAll().left(24));
        QVERIFY(registration.has_value());
        QCOMPARE(registration->type, Protocol::MessageType::REGISTER_HOST);
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
        QTRY_VERIFY(reconnectedPeer->bytesAvailable() >= 24);
        const auto newRegistration = Protocol::ProtocolSerializer::deserializeHeader(reconnectedPeer->readAll().left(24));
        QVERIFY(newRegistration.has_value());
        QCOMPARE(newRegistration->type, Protocol::MessageType::REGISTER_HOST);
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
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        const auto registration = Protocol::ProtocolSerializer::deserializeHeader(peer->readAll().left(24));
        QVERIFY(registration.has_value());
        QCOMPARE(registration->type, Protocol::MessageType::REGISTER_HOST);
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
        QTRY_VERIFY(peerA->bytesAvailable() >= 24);
        const auto registrationA = Protocol::ProtocolSerializer::deserializeHeader(peerA->readAll().left(24));
        QVERIFY(registrationA.has_value());
        QCOMPARE(registrationA->type, Protocol::MessageType::REGISTER_HOST);
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
        QTRY_VERIFY(peerB->bytesAvailable() >= 24);
        const auto registrationB = Protocol::ProtocolSerializer::deserializeHeader(peerB->readAll().left(24));
        QVERIFY(registrationB.has_value());
        QCOMPARE(registrationB->type, Protocol::MessageType::REGISTER_HOST);
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
        QTRY_VERIFY(peer->bytesAvailable() >= 24);
        const auto registration = Protocol::ProtocolSerializer::deserializeHeader(peer->readAll().left(24));
        QVERIFY(registration.has_value());
        QCOMPARE(registration->type, Protocol::MessageType::REGISTER_HOST);
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
        QTRY_VERIFY(reconnectedPeer->bytesAvailable() >= 24);
        const auto newRegistration = Protocol::ProtocolSerializer::deserializeHeader(reconnectedPeer->readAll().left(24));
        QVERIFY(newRegistration.has_value());
        QCOMPARE(newRegistration->type, Protocol::MessageType::REGISTER_HOST);
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
        QTRY_VERIFY(currentPeer->bytesAvailable() >= 24);
        const auto registration = Protocol::ProtocolSerializer::deserializeHeader(currentPeer->readAll().left(24));
        QVERIFY(registration.has_value());
        QCOMPARE(registration->type, Protocol::MessageType::REGISTER_HOST);
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

        const QByteArray authPayload = Protocol::relayAuthPayload(token, agentSessionId);
        Protocol::ProtocolHeader requestHeader(Protocol::MessageType::CONNECT_REQUEST);
        requestHeader.payloadLength = static_cast<uint32_t>(authPayload.size());
        const QByteArray expectedRequest =
                Protocol::ProtocolSerializer::serializeHeader(requestHeader) + authPayload;
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

        const QByteArray authPayload = Protocol::relayAuthPayload(token, agentSessionId);
        Protocol::ProtocolHeader requestHeader(Protocol::MessageType::CONNECT_REQUEST);
        requestHeader.payloadLength = static_cast<uint32_t>(authPayload.size());
        const QByteArray expectedRequest =
                Protocol::ProtocolSerializer::serializeHeader(requestHeader) + authPayload;
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
