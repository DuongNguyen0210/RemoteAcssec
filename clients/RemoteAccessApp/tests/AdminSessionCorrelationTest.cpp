#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>

#include "Network/Http/ApiClient.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Protocol/RelayAuthPayload.h"
#include "Network/Relay/AdminSessionController.h"

class AdminSessionCorrelationTest final : public QObject
{
    Q_OBJECT

private slots:
    void pendingARejectsBThenASuccessRetainsA();
};

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
