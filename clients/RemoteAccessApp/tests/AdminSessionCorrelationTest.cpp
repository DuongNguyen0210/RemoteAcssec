#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>

#include "Network/Http/ApiClient.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Protocol/RelayAuthPayload.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include "Network/Relay/AdminSessionController.h"
#include "Network/Relay/RelayEndpointProvider.h"

class AssignedProvider final : public RelayEndpointProvider {
public:
    QHash<QString, quint16> ports;
    void allocate(Callback callback) override { callback({}, "unused"); }
    void lookup(const QString &id, Callback callback) override {
        const auto port = ports.value(id);
        callback({"node-" + id, "127.0.0.1", port, 0}, {});
    }
};

class AdminSessionCorrelationTest final : public QObject
{
    Q_OBJECT

private slots:
    void pendingARejectsBThenASuccessRetainsA();
    void adminLooksUpEachAssignedRelay();
};

void AdminSessionCorrelationTest::adminLooksUpEachAssignedRelay()
{
    const QString agentA = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    const QString agentB = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
    QTcpServer first, second;
    QVERIFY(first.listen(QHostAddress::LocalHost, 0));
    QVERIFY(second.listen(QHostAddress::LocalHost, 0));
    auto *provider = new AssignedProvider;
    provider->ports.insert(agentA, first.serverPort());
    provider->ports.insert(agentB, second.serverPort());
    ApiClient::instance().setToken("test-token");
    AdminSessionController controller(provider, nullptr);
    QSignalSpy failures(&controller, &AdminSessionController::requestFailed);
    controller.requestSession(agentA);
    QTRY_VERIFY(first.hasPendingConnections());
    QVERIFY(!second.hasPendingConnections());
    auto *firstSocket = first.nextPendingConnection();
    controller.requestSession(agentB); // Pending A cannot be overwritten.
    firstSocket->write(Protocol::ProtocolSerializer::serializeHeader(Protocol::ProtocolHeader(Protocol::MessageType::CONNECT_RESULT)) + QByteArray(1, '\0'));
    firstSocket->flush();
    QTRY_COMPARE(failures.size(), 2);
    controller.requestSession(agentB);
    QTRY_VERIFY(second.hasPendingConnections());
    auto *secondSocket = second.nextPendingConnection();
    QVERIFY(secondSocket);
    const auto authPayload = Protocol::relayAuthPayload("test-token", agentB);
    Protocol::ProtocolHeader request(Protocol::MessageType::CONNECT_REQUEST);
    request.payloadLength = quint32(authPayload.size());
    const auto expectedRequest = Protocol::ProtocolSerializer::serializeHeader(request) + authPayload;
    QTRY_VERIFY(secondSocket->bytesAvailable() >= expectedRequest.size());
    QCOMPARE(secondSocket->read(expectedRequest.size()), expectedRequest);
    Protocol::ProtocolHeader accepted(Protocol::MessageType::CONNECT_RESULT);
    accepted.sessionId = 73;
    accepted.payloadLength = 1;
    secondSocket->write(Protocol::ProtocolSerializer::serializeHeader(accepted) + QByteArray(1, '\1'));
    secondSocket->flush();
    QSignalSpy established(&controller, &AdminSessionController::sessionEstablished);
    QTRY_COMPARE(established.size(), 1);
    const auto inputPayload = RemoteInputPacket::key(Qt::Key_A);
    QVERIFY(controller.sendInput(Protocol::MessageType::KEY_PRESS, inputPayload));
    Protocol::ProtocolHeader input(Protocol::MessageType::KEY_PRESS);
    input.sessionId = 73;
    input.payloadLength = quint32(inputPayload.size());
    const auto expectedInput = Protocol::ProtocolSerializer::serializeHeader(input) + inputPayload;
    QTRY_VERIFY(secondSocket->bytesAvailable() >= expectedInput.size());
    QCOMPARE(secondSocket->read(expectedInput.size()), expectedInput);
    ApiClient::instance().setToken({});
}

void AdminSessionCorrelationTest::pendingARejectsBThenASuccessRetainsA()
{
    const QString agentA = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
    const QString agentB = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb");
    const QString token = QStringLiteral("f1-test-token");

    QTcpServer fakeRelay;
    QVERIFY(fakeRelay.listen(QHostAddress::LocalHost, 0));
    qputenv("REMOTE_RELAY_HOST", "127.0.0.1");
    qputenv("REMOTE_RELAY_PORT", QByteArray::number(fakeRelay.serverPort()));
    ApiClient::instance().setToken(token);

    AdminSessionController controller;
    QSignalSpy failureSpy(&controller, &AdminSessionController::requestFailed);
    QSignalSpy establishedSpy(&controller, &AdminSessionController::sessionEstablished);
    QVERIFY(failureSpy.isValid());
    QVERIFY(establishedSpy.isValid());

    controller.requestSession(agentA);
    QTRY_VERIFY_WITH_TIMEOUT(fakeRelay.hasPendingConnections(), 5000);
    QTcpSocket *relaySocket = fakeRelay.nextPendingConnection();
    QVERIFY(relaySocket != nullptr);

    const QByteArray authPayload = Protocol::relayAuthPayload(token, agentA);
    Protocol::ProtocolHeader requestHeader(Protocol::MessageType::CONNECT_REQUEST);
    requestHeader.payloadLength = static_cast<uint32_t>(authPayload.size());
    const QByteArray expectedRequest =
        Protocol::ProtocolSerializer::serializeHeader(requestHeader) + authPayload;
    QTRY_VERIFY_WITH_TIMEOUT(relaySocket->bytesAvailable() >= expectedRequest.size(), 5000);
    QCOMPARE(relaySocket->read(expectedRequest.size()), expectedRequest);

    controller.requestSession(agentB);
    QCOMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.at(0).at(0).toString(), agentB);
    QVERIFY(!failureSpy.at(0).at(1).toString().isEmpty());
    QCOMPARE(establishedSpy.count(), 0);

    Protocol::ProtocolHeader resultHeader(Protocol::MessageType::CONNECT_RESULT);
    resultHeader.sessionId = 42;
    resultHeader.payloadLength = 1;
    const QByteArray resultPacket =
        Protocol::ProtocolSerializer::serializeHeader(resultHeader) + QByteArray(1, '\1');
    QCOMPARE(relaySocket->write(resultPacket), qint64(resultPacket.size()));
    QVERIFY(relaySocket->flush());

    QTRY_COMPARE_WITH_TIMEOUT(establishedSpy.count(), 1, 5000);
    QCOMPARE(establishedSpy.at(0).at(0).toULongLong(), quint64{42});
    QCOMPARE(establishedSpy.at(0).at(1).toString(), agentA);
    QCOMPARE(failureSpy.count(), 1);

    ApiClient::instance().setToken({});
    qunsetenv("REMOTE_RELAY_HOST");
    qunsetenv("REMOTE_RELAY_PORT");
}

QTEST_GUILESS_MAIN(AdminSessionCorrelationTest)

#include "AdminSessionCorrelationTest.moc"
