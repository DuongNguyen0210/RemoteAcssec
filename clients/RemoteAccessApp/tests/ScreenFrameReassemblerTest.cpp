#include <QtTest>
#include <QtEndian>
#include "Network/Protocol/ScreenFrameReassembler.h"

class ScreenFrameReassemblerTest final : public QObject
{
    Q_OBJECT

private slots:
    void singleChunkFrameReassemblesCorrectly();
    void multiChunkInOrderReassemblesCorrectly();
    void multiChunkOutOfOrderReassemblesCorrectly();
    void duplicateChunksAreIgnored();
    void invalidMetadataIsRejected();
    void inFlightEvictionWhenLimitExceeded();
    void clearResetsAllState();

private:
    static QByteArray makePayload(quint32 frameId, quint32 chunkIndex, quint32 chunkCount,
                                  quint32 totalFrameSize, const QByteArray &chunkData)
    {
        QByteArray payload;
        payload.resize(16 + chunkData.size());
        auto *ptr = reinterpret_cast<uchar *>(payload.data());
        qToBigEndian<quint32>(frameId, ptr);
        qToBigEndian<quint32>(chunkIndex, ptr + 4);
        qToBigEndian<quint32>(chunkCount, ptr + 8);
        qToBigEndian<quint32>(totalFrameSize, ptr + 12);
        memcpy(payload.data() + 16, chunkData.constData(), chunkData.size());
        return payload;
    }
};

void ScreenFrameReassemblerTest::singleChunkFrameReassemblesCorrectly()
{
    Protocol::ScreenFrameReassembler reassembler;
    const QByteArray jpegMock = "fake_jpeg_data_bytes_12345";

    const QByteArray payload = makePayload(101, 0, 1, jpegMock.size(), jpegMock);
    const auto result = reassembler.feed(payload);

    QVERIFY(result.has_value());
    QCOMPARE(result->frameId, 101U);
    QCOMPARE(result->data, jpegMock);
    QCOMPARE(reassembler.inFlightCount(), 0);
}

void ScreenFrameReassemblerTest::multiChunkInOrderReassemblesCorrectly()
{
    Protocol::ScreenFrameReassembler reassembler;
    const QByteArray part1 = "AAAA";
    const QByteArray part2 = "BBBB";
    const QByteArray part3 = "CCCC";
    const quint32 totalSize = part1.size() + part2.size() + part3.size();

    QCOMPARE(reassembler.feed(makePayload(202, 0, 3, totalSize, part1)), std::nullopt);
    QCOMPARE(reassembler.inFlightCount(), 1);

    QCOMPARE(reassembler.feed(makePayload(202, 1, 3, totalSize, part2)), std::nullopt);
    QCOMPARE(reassembler.inFlightCount(), 1);

    const auto result = reassembler.feed(makePayload(202, 2, 3, totalSize, part3));
    QVERIFY(result.has_value());
    QCOMPARE(result->frameId, 202U);
    QCOMPARE(result->data, QByteArray("AAAABBBBCCCC"));
    QCOMPARE(reassembler.inFlightCount(), 0);
}

void ScreenFrameReassemblerTest::multiChunkOutOfOrderReassemblesCorrectly()
{
    Protocol::ScreenFrameReassembler reassembler;
    const QByteArray part1 = "111";
    const QByteArray part2 = "222";
    const QByteArray part3 = "333";
    const quint32 totalSize = 9;

    // Receive order: 2 -> 0 -> 1
    QCOMPARE(reassembler.feed(makePayload(303, 2, 3, totalSize, part3)), std::nullopt);
    QCOMPARE(reassembler.feed(makePayload(303, 0, 3, totalSize, part1)), std::nullopt);

    const auto result = reassembler.feed(makePayload(303, 1, 3, totalSize, part2));
    QVERIFY(result.has_value());
    QCOMPARE(result->frameId, 303U);
    QCOMPARE(result->data, QByteArray("111222333"));
    QCOMPARE(reassembler.inFlightCount(), 0);
}

void ScreenFrameReassemblerTest::duplicateChunksAreIgnored()
{
    Protocol::ScreenFrameReassembler reassembler;
    const QByteArray part1 = "CHUNK0";
    const QByteArray part2 = "CHUNK1";
    const quint32 totalSize = part1.size() + part2.size();

    QCOMPARE(reassembler.feed(makePayload(404, 0, 2, totalSize, part1)), std::nullopt);
    // Duplicate of chunk 0
    QCOMPARE(reassembler.feed(makePayload(404, 0, 2, totalSize, part1)), std::nullopt);
    QCOMPARE(reassembler.inFlightCount(), 1);

    const auto result = reassembler.feed(makePayload(404, 1, 2, totalSize, part2));
    QVERIFY(result.has_value());
    QCOMPARE(result->frameId, 404U);
    QCOMPARE(result->data, QByteArray("CHUNK0CHUNK1"));
}

void ScreenFrameReassemblerTest::invalidMetadataIsRejected()
{
    Protocol::ScreenFrameReassembler reassembler;

    // 1. Shorter than 16 bytes
    QCOMPARE(reassembler.feed(QByteArray(15, 'x')), std::nullopt);

    // 2. chunkCount == 0
    QCOMPARE(reassembler.feed(makePayload(501, 0, 0, 10, "data")), std::nullopt);

    // 3. chunkIndex >= chunkCount
    QCOMPARE(reassembler.feed(makePayload(502, 3, 2, 10, "data")), std::nullopt);

    // 4. totalFrameSize == 0
    QCOMPARE(reassembler.feed(makePayload(503, 0, 1, 0, "data")), std::nullopt);

    // 5. empty chunk data (payload size == 16)
    QCOMPARE(reassembler.feed(makePayload(504, 0, 1, 10, "")), std::nullopt);

    QCOMPARE(reassembler.inFlightCount(), 0);
}

void ScreenFrameReassemblerTest::inFlightEvictionWhenLimitExceeded()
{
    Protocol::ScreenFrameReassembler reassembler;

    // Send 1 chunk of 4 different 2-chunk frames: 1, 2, 3, 4
    reassembler.feed(makePayload(1, 0, 2, 10, "A"));
    reassembler.feed(makePayload(2, 0, 2, 10, "B"));
    reassembler.feed(makePayload(3, 0, 2, 10, "C"));
    reassembler.feed(makePayload(4, 0, 2, 10, "D"));
    QCOMPARE(reassembler.inFlightCount(), 4);

    // Send chunk for 5th frame: frame 1 should be evicted!
    reassembler.feed(makePayload(5, 0, 2, 10, "E"));
    QCOMPARE(reassembler.inFlightCount(), 4);

    // Now send chunk 1 for frame 1: frame 1 was already evicted, so this starts as a new frame state
    // rather than completing the previous one!
    const auto lateResult = reassembler.feed(makePayload(1, 1, 2, 10, "late"));
    QCOMPARE(lateResult, std::nullopt);
}

void ScreenFrameReassemblerTest::clearResetsAllState()
{
    Protocol::ScreenFrameReassembler reassembler;
    reassembler.feed(makePayload(1, 0, 2, 10, "data"));
    reassembler.feed(makePayload(2, 0, 2, 10, "data"));
    QCOMPARE(reassembler.inFlightCount(), 2);

    reassembler.clear();
    QCOMPARE(reassembler.inFlightCount(), 0);
}

QTEST_MAIN(ScreenFrameReassemblerTest)
#include "ScreenFrameReassemblerTest.moc"
