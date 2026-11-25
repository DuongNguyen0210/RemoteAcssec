#include "Streaming/ScreenEncoder.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QRandomGenerator>
#include <QTest>

#include <algorithm>
#include <set>
#include <tuple>
#include <vector>

namespace {
QSize scaledSize(const QSize &size, int percent)
{
    return QSize(qMax(1, qRound(size.width() * percent / 100.0)),
                 qMax(1, qRound(size.height() * percent / 100.0)));
}

struct Probe {
    QSize size;
    int quality;
    qsizetype bytes;
};

auto rank(const Probe &candidate)
{
    return std::make_tuple(qint64(candidate.size.width()) * candidate.size.height(),
                           candidate.quality);
}

// Convert either a real JPEG or a simulated offer into the same observable record.
Probe highestFitting(const std::vector<Probe> &probes, qsizetype budget)
{
    Probe best{QSize(0, 0), 0, 0};
    for (const auto &probe : probes) {
        if (probe.bytes > 0 && probe.bytes <= budget && rank(probe) > rank(best))
            best = probe;
    }
    return best;
}

std::vector<Probe> exhaustiveGrid(const QImage &image)
{
    std::vector<Probe> probes;
    for (int percent = 100; percent >= 10; percent -= 5) {
        const QImage candidate = image.scaled(scaledSize(image.size(), percent),
                                              Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        for (int quality = 60; quality >= 45; quality -= 5) {
            const QByteArray jpeg = ScreenEncoder::encodeJpeg(candidate, quality);
            probes.push_back({candidate.size(), quality, jpeg.size()});
        }
    }
    return probes;
}

QImage scene(const QSize &size, const QString &kind, int scrollOffset)
{
    QImage image(size, QImage::Format_RGB32);
    if (kind == QStringLiteral("detail")) {
        QRandomGenerator random(2048);
        for (int y = 0; y < size.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < size.width(); ++x) {
                line[x] = 0xff000000u | random.generate();
            }
        }
        return image;
    }

    image.fill(QColor(246, 247, 249));
    QPainter painter(&image);
    painter.setFont(QFont(QStringLiteral("DejaVu Sans Mono"), 11));
    const bool scroll = kind == QStringLiteral("scroll");
    const int lineSpacing = scroll ? 19 : 34;
    int row = 0;
    for (int y = 0; y < size.height() + 40; y += lineSpacing, ++row) {
        for (int x = 8; x < size.width(); x += 510) {
            painter.setPen(QColor(32, 45 + row % 80, 64));
            painter.drawText(x, y - scrollOffset,
                             QStringLiteral("%1   if (ready) return render(frame, bounds);")
                                 .arg(100 + row + x));
        }
    }
    return image;
}

std::vector<Probe> measured;
bool failEncoding = false;

QByteArray simulatedEncoder(const QImage &image, int quality)
{
    // The larger 80% profile and quality 55 each introduce a size inversion.
    const int percent = image.width() / 2;
    int bytes = percent * quality / 10;
    if (percent == 80) bytes -= 85;
    if (quality == 55) bytes -= 45;
    const QByteArray data = failEncoding ? QByteArray() : QByteArray(qMax(2, bytes), 'x');
    measured.push_back({image.size(), quality, data.size()});
    return data;
}

double percentile(std::vector<double> sorted, int percent)
{
    std::sort(sorted.begin(), sorted.end());
    const auto position = (sorted.size() * percent + 99) / 100;
    return sorted.at(position - 1);
}
}

class AdaptiveScreenEncoderTest : public QObject
{
    Q_OBJECT
private slots:
    void ranksMeasuredTrialsAndCountsEveryCall()
    {
        QImage image(200, 100, QImage::Format_RGB32);
        image.fill(Qt::white);
        ScreenEncodeState state;
        for (qsizetype budget : {460, 370, 280, 600, 75, 1}) {
            for (int frame = 0; frame < 16; ++frame) {
                measured.clear();
                const auto encoded = ScreenEncoder::encodeWithinLimit(
                    image, budget, state, frame % 4 == 0, simulatedEncoder);
                state = encoded.nextState;
                QCOMPARE(size_t(encoded.attempts), measured.size());
                QVERIFY(encoded.attempts >= 1 && encoded.attempts <= 3);
                const auto best = highestFitting(measured, budget);
                if (best.bytes == 0) {
                    QVERIFY(encoded.jpeg.isEmpty());
                } else {
                    QCOMPARE(encoded.size, best.size);
                    QCOMPARE(encoded.quality, best.quality);
                    QCOMPARE(encoded.jpeg.size(), best.bytes);
                }
            }
        }
        failEncoding = true;
        measured.clear();
        const auto failed = ScreenEncoder::encodeWithinLimit(image, 1000, state, true, simulatedEncoder);
        failEncoding = false;
        QVERIFY(failed.jpeg.isEmpty());
        QCOMPARE(size_t(failed.attempts), measured.size());
        QVERIFY(failed.attempts >= 1 && failed.attempts <= 3);
    }

    void reusesFittingProfileAndHonorsExactBudget()
    {
        QImage image(200, 100, QImage::Format_RGB32);
        image.fill(Qt::black);
        measured.clear();
        const auto native = ScreenEncoder::encodeWithinLimit(image, 600, {}, false, simulatedEncoder);
        QCOMPARE(native.size, image.size());
        QCOMPARE(native.quality, 60);
        QCOMPARE(native.jpeg.size(), 600);
        QCOMPARE(measured.size(), size_t(1));
        measured.clear();
        const auto warm = ScreenEncoder::encodeWithinLimit(image, 600, native.nextState, false, simulatedEncoder);
        QCOMPARE(warm.jpeg.size(), native.jpeg.size());
        QCOMPARE(measured.size(), size_t(1));
        measured.clear();
        const auto tight = ScreenEncoder::encodeWithinLimit(image, 599, warm.nextState, false, simulatedEncoder);
        QVERIFY(!tight.jpeg.isEmpty());
        QVERIFY(tight.jpeg.size() <= 599);
        QVERIFY(measured.size() <= 3);
    }

    void tinyCapturesReuseEquivalentCandidates()
    {
        QImage image(1, 2, QImage::Format_RGB32);
        image.fill(Qt::black);
        ScreenEncodeState state;
        for (int tick = 0; tick < 8; ++tick) {
            measured.clear();
            // Reject every probe so rounding brings equivalent trial sizes into play.
            const auto frame = ScreenEncoder::encodeWithinLimit(image, 1, state, true, simulatedEncoder);
            state = frame.nextState;
            std::set<std::tuple<int, int, int>> profiles;
            for (const auto &trial : measured) {
                QVERIFY(profiles.emplace(trial.size.width(), trial.size.height(), trial.quality).second);
            }
            QVERIFY(frame.jpeg.isEmpty());
        }
    }

    void revisitsHigherProfilesAfterAnInvertedSizeMeasurement()
    {
        QImage image(200, 100, QImage::Format_RGB32);
        image.fill(Qt::black);
        auto encode = [](const QImage &candidate, int quality) {
            const bool fits = candidate.width() <= 90 || (candidate.width() == 160 && quality == 55);
            return QByteArray(fits ? 100 : 1000, 'x');
        };
        ScreenEncodeState state;
        for (int opportunity = 0; opportunity < 500; ++opportunity) {
            const auto result = ScreenEncoder::encodeWithinLimit(image, 150, state, true, encode);
            state = result.nextState;
            QVERIFY(result.attempts <= 3);
            if (result.size == QSize(160, 80) && result.quality == 55) return;
        }
        QFAIL("A failed lower-resolution trial permanently hid a better feasible profile");
    }

    void coldRecoveryCanFindAnIsolatedFitWithoutHealthyHistory()
    {
        QImage image(200, 100, QImage::Format_RGB32);
        image.fill(Qt::black);
        auto encode = [](const QImage &candidate, int quality) {
            return QByteArray(candidate.width() == 160 && quality == 55 ? 100 : 1000, 'x');
        };
        ScreenEncodeState state;
        for (int capture = 0; capture < 80; ++capture) {
            const auto result = ScreenEncoder::encodeWithinLimit(image, 150, state, false, encode);
            QVERIFY(result.attempts <= 3);
            if (!result.jpeg.isEmpty()) {
                QCOMPARE(result.size, QSize(160, 80));
                QCOMPARE(result.quality, 55);
                return;
            }
            state = result.nextState;
        }
        QFAIL("Cold recovery stalled at an infeasible minimum profile");
    }

    void stableSceneAgainstExhaustiveGrid_data()
    {
        QTest::addColumn<QSize>("size");
        QTest::addColumn<QString>("kind");
        for (const QSize size : {QSize(1920, 1080), QSize(3840, 2160)}) {
            for (const QString &kind : {QStringLiteral("text"), QStringLiteral("scroll"),
                                        QStringLiteral("detail")}) {
                const auto label = QStringLiteral("%1p-%2").arg(size.height()).arg(kind).toLatin1();
                QTest::newRow(label.constData()) << size << kind;
            }
        }
    }

    void stableSceneAgainstExhaustiveGrid()
    {
        QFETCH(QSize, size);
        QFETCH(QString, kind);
        const QImage primary = scene(size, kind, 0);
        const QImage alternate = kind == QStringLiteral("scroll") ? scene(size, kind, 8) : primary;
        constexpr qsizetype jpegBudget = 62500 - 40; // 1 Mbps at the 2 FPS floor.
        const auto offers = exhaustiveGrid(primary);
        const auto optimum = highestFitting(offers, jpegBudget);
        QVERIFY(optimum.bytes > 0);
        const auto firstFit = std::find_if(offers.begin(), offers.end(), [](const Probe &probe) {
            return probe.bytes > 0 && probe.bytes <= jpegBudget;
        });
        QVERIFY(firstFit != offers.end());
        QCOMPARE(rank(*firstFit) == rank(optimum), true);

        ScreenEncodeState state;
        EncodedScreenFrame last;
        std::vector<double> milliseconds;
        int maxAttempts = 0;
        // A deterministic two-FPS capture clock, with an upgrade opportunity every 2 s.
        for (int tick = 0; tick <= 20; ++tick) {
            const QImage &capture = tick % 2 == 0 ? primary : alternate;
            const bool probe = tick > 0 && tick % 4 == 0;
            QElapsedTimer timer;
            timer.start();
            last = ScreenEncoder::encodeWithinLimit(capture, jpegBudget, state, probe);
            milliseconds.push_back(timer.nsecsElapsed() / 1e6);
            maxAttempts = std::max(maxAttempts, last.attempts);
            QVERIFY(last.attempts >= 1 && last.attempts <= 3);
            QVERIFY(last.jpeg.size() <= jpegBudget);
            state = last.nextState;
        }

        qInfo().nospace() << size << ' ' << kind << " measured=" << last.size
                          << '/' << last.quality << " oracle=" << optimum.size << '/' << optimum.quality
                          << " bytes=" << last.jpeg.size() << " maxCalls=" << maxAttempts
                          << " p50ms=" << percentile(milliseconds, 50)
                          << " p95ms=" << percentile(milliseconds, 95);
        QVERIFY(!last.jpeg.isEmpty());
        QCOMPARE(QImage::fromData(last.jpeg, "JPEG").size(), last.size);
        QVERIFY(last.size.width() <= optimum.size.width());
        QVERIFY(last.size.width() >= optimum.size.width() - qCeil(size.width() * 0.05));
        if (last.size == optimum.size) {
            QVERIFY(last.quality <= optimum.quality);
            QVERIFY(last.quality >= optimum.quality - 5);
        }
    }
};

QTEST_MAIN(AdaptiveScreenEncoderTest)
#include "AdaptiveScreenEncoderTest.moc"
