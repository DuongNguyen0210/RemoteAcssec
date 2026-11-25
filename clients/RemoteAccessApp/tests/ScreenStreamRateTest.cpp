#include <QtTest>
#include "Streaming/ScreenStreamRate.h"

class ScreenStreamRateTest : public QObject
{
    Q_OBJECT
private slots:
    void allowsOneSecondOfBytesUpToTheProtocolLimit()
    {
        ScreenStreamRate rate(125000);
        QCOMPARE(rate.wireBudget(), 125000);
        rate.sent(0, 94703);
        QCOMPARE(rate.nextSendMs(), 758);
        QCOMPARE(rate.wireBudget(), 125000);

        ScreenStreamRate fast(1500000);
        QCOMPARE(fast.wireBudget(), ScreenStreamRate::MaxWireFrameBytes);
        fast.sent(500, 1000);
        QCOMPARE(fast.nextSendMs(), 600);
    }

    void bytePacingBoundsEveryWindowWithVariableSizes()
    {
        ScreenStreamRate rate(20000);
        const QList<int> frames{510, 10231, 20000, 8192, 1301, 10000, 512};
        QList<qint64> sentAt;
        for (const int bytes : frames) {
            const qint64 now = rate.nextSendMs();
            sentAt.append(now);
            rate.sent(now, bytes);
            QVERIFY(rate.nextSendMs() > now);
            for (int start = 0; start < sentAt.size(); ++start) {
                int total = 0;
                int largest = 0;
                for (int i = start; i < sentAt.size(); ++i) {
                    total += frames.at(i);
                    largest = qMax(largest, frames.at(i));
                }
                QVERIFY(total <= (now - sentAt.at(start)) * rate.bytesPerSecond() / 1000 + largest);
            }
        }
    }

    void sparseFramesCanStillProbeAndResetClearsTheirTiming()
    {
        ScreenStreamRate rate(125000);
        QVERIFY(!rate.canProbe(5000));
        rate.sent(6000, 121000);
        QVERIFY(!rate.canProbe(7999));
        QVERIFY(rate.canProbe(8000));
        rate.sent(8200, 121000);
        QCOMPARE(rate.wireBudget(), 125000);
        QVERIFY(rate.canProbe(8200));

        rate.reset();
        QCOMPARE(rate.lastSendMs(), -1);
        QCOMPARE(rate.nextSendMs(), 0);
        QCOMPARE(rate.wireBudget(), 125000);
        QVERIFY(!rate.canProbe(9000));
    }
};

QTEST_APPLESS_MAIN(ScreenStreamRateTest)
#include "ScreenStreamRateTest.moc"
