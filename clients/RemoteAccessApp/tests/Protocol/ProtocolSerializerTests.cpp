#include <QTest>
#include <QByteArray>
#include "ProtocolConstants.h"
#include "ProtocolHeader.h"
#include "ProtocolSerializer.h"

using namespace Protocol;

class ProtocolSerializerTests : public QObject
{
    Q_OBJECT

private slots:
    void testDefaults()
    {
        ProtocolHeader header(MessageType::PING);
        QCOMPARE(header.magic, PROTOCOL_MAGIC);
        QCOMPARE(header.version, PROTOCOL_VERSION);
        QCOMPARE(header.type, MessageType::PING);
        QCOMPARE(header.flags, 0);
        QCOMPARE(header.payloadLength, 0);
        QCOMPARE(header.sessionId, 0ULL);
        QCOMPARE(header.sequenceNumber, 0);
    }

    void testExactSerialization()
    {
        ProtocolHeader header(MessageType::PING); // 0x41
        // magic and version are already set to 0x52445450 and 0x01
        header.flags = 0x1234;
        header.payloadLength = 0x01020304;
        header.sessionId = 0x0102030405060708ULL;
        header.sequenceNumber = 0x11223344;

        QByteArray data = ProtocolSerializer::serializeHeader(header);
        QCOMPARE(data.size(), static_cast<int>(HEADER_SIZE));

        QByteArray expected;
        expected.append(static_cast<char>(0x52));
        expected.append(static_cast<char>(0x44));
        expected.append(static_cast<char>(0x54));
        expected.append(static_cast<char>(0x50));
        expected.append(static_cast<char>(0x01));
        expected.append(static_cast<char>(0x41));
        expected.append(static_cast<char>(0x12));
        expected.append(static_cast<char>(0x34));
        expected.append(static_cast<char>(0x01));
        expected.append(static_cast<char>(0x02));
        expected.append(static_cast<char>(0x03));
        expected.append(static_cast<char>(0x04));
        expected.append(static_cast<char>(0x01));
        expected.append(static_cast<char>(0x02));
        expected.append(static_cast<char>(0x03));
        expected.append(static_cast<char>(0x04));
        expected.append(static_cast<char>(0x05));
        expected.append(static_cast<char>(0x06));
        expected.append(static_cast<char>(0x07));
        expected.append(static_cast<char>(0x08));
        expected.append(static_cast<char>(0x11));
        expected.append(static_cast<char>(0x22));
        expected.append(static_cast<char>(0x33));
        expected.append(static_cast<char>(0x44));

        QCOMPARE(data, expected);
    }

    void testDeserialization()
    {
        QByteArray data;
        data.append(static_cast<char>(0x52));
        data.append(static_cast<char>(0x44));
        data.append(static_cast<char>(0x54));
        data.append(static_cast<char>(0x50));
        data.append(static_cast<char>(0x01));
        data.append(static_cast<char>(0x41));
        data.append(static_cast<char>(0x12));
        data.append(static_cast<char>(0x34));
        data.append(static_cast<char>(0x01));
        data.append(static_cast<char>(0x02));
        data.append(static_cast<char>(0x03));
        data.append(static_cast<char>(0x04));
        data.append(static_cast<char>(0x01));
        data.append(static_cast<char>(0x02));
        data.append(static_cast<char>(0x03));
        data.append(static_cast<char>(0x04));
        data.append(static_cast<char>(0x05));
        data.append(static_cast<char>(0x06));
        data.append(static_cast<char>(0x07));
        data.append(static_cast<char>(0x08));
        data.append(static_cast<char>(0x11));
        data.append(static_cast<char>(0x22));
        data.append(static_cast<char>(0x33));
        data.append(static_cast<char>(0x44));

        std::optional<ProtocolHeader> result = ProtocolSerializer::deserializeHeader(data);
        QVERIFY(result.has_value());

        QCOMPARE(result->magic, 0x52445450U);
        QCOMPARE(result->version, 0x01U);
        QCOMPARE(result->type, MessageType::PING);
        QCOMPARE(result->flags, 0x1234U);
        QCOMPARE(result->payloadLength, 0x01020304U);
        QCOMPARE(result->sessionId, 0x0102030405060708ULL);
        QCOMPARE(result->sequenceNumber, 0x11223344U);
    }

    void testRoundTrip()
    {
        ProtocolHeader original(MessageType::SCREEN_FRAME);
        original.flags = 0x9999;
        original.payloadLength = 123456;
        original.sessionId = 9876543210ULL;
        original.sequenceNumber = 42;

        QByteArray data = ProtocolSerializer::serializeHeader(original);
        std::optional<ProtocolHeader> result = ProtocolSerializer::deserializeHeader(data);
        
        QVERIFY(result.has_value());
        QCOMPARE(result->magic, original.magic);
        QCOMPARE(result->version, original.version);
        QCOMPARE(result->type, original.type);
        QCOMPARE(result->flags, original.flags);
        QCOMPARE(result->payloadLength, original.payloadLength);
        QCOMPARE(result->sessionId, original.sessionId);
        QCOMPARE(result->sequenceNumber, original.sequenceNumber);
    }

    void testInvalidSizes()
    {
        QByteArray data0;
        QVERIFY(!ProtocolSerializer::deserializeHeader(data0).has_value());

        QByteArray data23(23, '\0');
        QVERIFY(!ProtocolSerializer::deserializeHeader(data23).has_value());

        QByteArray data25(25, '\0');
        QVERIFY(!ProtocolSerializer::deserializeHeader(data25).has_value());
    }

    void testHighBytes()
    {
        ProtocolHeader original(MessageType::ERROR); // 0x70
        original.flags = 0x80FF;
        original.payloadLength = 0x89ABCDEF;
        original.sessionId = 0xFEDCBA9876543210ULL;
        original.sequenceNumber = 0xFFEEDDCC;

        QByteArray data = ProtocolSerializer::serializeHeader(original);
        std::optional<ProtocolHeader> result = ProtocolSerializer::deserializeHeader(data);

        QVERIFY(result.has_value());
        QCOMPARE(result->magic, original.magic);
        QCOMPARE(result->version, original.version);
        QCOMPARE(result->type, original.type);
        QCOMPARE(result->flags, original.flags);
        QCOMPARE(result->payloadLength, original.payloadLength);
        QCOMPARE(result->sessionId, original.sessionId);
        QCOMPARE(result->sequenceNumber, original.sequenceNumber);
    }
};

QTEST_APPLESS_MAIN(ProtocolSerializerTests)
#include "ProtocolSerializerTests.moc"
