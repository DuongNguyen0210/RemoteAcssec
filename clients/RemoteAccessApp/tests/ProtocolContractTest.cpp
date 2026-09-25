#include <QtTest>

#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Protocol/RdtpStreamParser.h"

class ProtocolContractTest final : public QObject
{
    Q_OBJECT

private slots:
    void headerUsesCurrent24ByteBigEndianWireFormat();
    void parserBuffersFragmentedTcpInputUntilMessageIsComplete();
    void parserEmitsAllMessagesFromCoalescedTcpInput();
};

void ProtocolContractTest::headerUsesCurrent24ByteBigEndianWireFormat()
{
    Protocol::ProtocolHeader header(Protocol::MessageType::SCREEN_FRAME);
    header.flags = 0x1234U;
    header.payloadLength = 0x00010203U;
    header.sessionId = 0x0102030405060708ULL;
    header.sequenceNumber = 0xA1B2C3D4U;

    const QByteArray expectedBytes = QByteArray::fromHex(
        "52445450"
        "02"
        "10"
        "1234"
        "00010203"
        "0102030405060708"
        "a1b2c3d4"
        );

    const QByteArray serialized =
        Protocol::ProtocolSerializer::serializeHeader(header);

    QCOMPARE(serialized.size(), static_cast<int>(Protocol::HEADER_SIZE));
    QCOMPARE(serialized, expectedBytes);

    const auto decoded =
        Protocol::ProtocolSerializer::deserializeHeader(expectedBytes);

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->magic, header.magic);
    QCOMPARE(decoded->version, header.version);
    QCOMPARE(static_cast<int>(decoded->type),
             static_cast<int>(header.type));
    QCOMPARE(decoded->flags, header.flags);
    QCOMPARE(decoded->payloadLength, header.payloadLength);
    QCOMPARE(decoded->sessionId, header.sessionId);
    QCOMPARE(decoded->sequenceNumber, header.sequenceNumber);
}

void ProtocolContractTest::
    parserBuffersFragmentedTcpInputUntilMessageIsComplete()
{
    const QByteArray payload = QByteArray::fromHex("010203");

    Protocol::ProtocolHeader header(Protocol::MessageType::CONNECT_RESULT);
    header.payloadLength = static_cast<uint32_t>(payload.size());
    header.sessionId = 0x0102030405060708ULL;
    header.sequenceNumber = 7U;

    QByteArray packet =
        Protocol::ProtocolSerializer::serializeHeader(header);
    packet.append(payload);

    Protocol::RdtpStreamParser parser;

    const auto first = parser.feed(packet.left(5));
    QCOMPARE(static_cast<int>(first.error),
             static_cast<int>(Protocol::RdtpStreamParser::Error::None));
    QVERIFY(first.messages.isEmpty());

    const int remainingHeaderBytes =
        static_cast<int>(Protocol::HEADER_SIZE) - 5;

    const auto second =
        parser.feed(packet.mid(5, remainingHeaderBytes));
    QCOMPARE(static_cast<int>(second.error),
             static_cast<int>(Protocol::RdtpStreamParser::Error::None));
    QVERIFY(second.messages.isEmpty());

    const auto third =
        parser.feed(packet.mid(static_cast<int>(Protocol::HEADER_SIZE), 1));
    QCOMPARE(static_cast<int>(third.error),
             static_cast<int>(Protocol::RdtpStreamParser::Error::None));
    QVERIFY(third.messages.isEmpty());

    const auto fourth =
        parser.feed(packet.mid(static_cast<int>(Protocol::HEADER_SIZE) + 1));
    QCOMPARE(static_cast<int>(fourth.error),
             static_cast<int>(Protocol::RdtpStreamParser::Error::None));
    QCOMPARE(fourth.messages.size(), 1);

    const auto &message = fourth.messages.first();
    QCOMPARE(static_cast<int>(message.header.type),
             static_cast<int>(header.type));
    QCOMPARE(message.header.payloadLength, header.payloadLength);
    QCOMPARE(message.header.sessionId, header.sessionId);
    QCOMPARE(message.header.sequenceNumber, header.sequenceNumber);
    QCOMPARE(message.payload, payload);
}

void ProtocolContractTest::
    parserEmitsAllMessagesFromCoalescedTcpInput()
{
    const auto makePacket =
        [](Protocol::MessageType type,
           const QByteArray &payload,
           uint64_t sessionId,
           uint32_t sequenceNumber)
    {
        Protocol::ProtocolHeader header(type);
        header.payloadLength = static_cast<uint32_t>(payload.size());
        header.sessionId = sessionId;
        header.sequenceNumber = sequenceNumber;

        QByteArray packet =
            Protocol::ProtocolSerializer::serializeHeader(header);
        packet.append(payload);
        return packet;
    };

    const QByteArray firstPayload = QByteArray::fromHex("aabb");
    const QByteArray secondPayload = QByteArray::fromHex("01020304");

    QByteArray tcpBytes = makePacket(
        Protocol::MessageType::PING,
        firstPayload,
        11U,
        3U
    );

    tcpBytes.append(makePacket(
        Protocol::MessageType::PONG,
        secondPayload,
        22U,
        4U
    ));

    Protocol::RdtpStreamParser parser;
    const auto result = parser.feed(tcpBytes);

    QCOMPARE(static_cast<int>(result.error),
             static_cast<int>(Protocol::RdtpStreamParser::Error::None));
    QCOMPARE(result.messages.size(), 2);

    const auto &firstMessage = result.messages.at(0);
    QCOMPARE(static_cast<int>(firstMessage.header.type),
             static_cast<int>(Protocol::MessageType::PING));
    QCOMPARE(firstMessage.header.sessionId, uint64_t{11});
    QCOMPARE(firstMessage.header.sequenceNumber, uint32_t{3});
    QCOMPARE(firstMessage.payload, firstPayload);

    const auto &secondMessage = result.messages.at(1);
    QCOMPARE(static_cast<int>(secondMessage.header.type),
             static_cast<int>(Protocol::MessageType::PONG));
    QCOMPARE(secondMessage.header.sessionId, uint64_t{22});
    QCOMPARE(secondMessage.header.sequenceNumber, uint32_t{4});
    QCOMPARE(secondMessage.payload, secondPayload);
}

QTEST_APPLESS_MAIN(ProtocolContractTest)

#include "ProtocolContractTest.moc"
