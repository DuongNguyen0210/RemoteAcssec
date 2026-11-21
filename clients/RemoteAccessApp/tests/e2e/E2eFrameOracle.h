#pragma once

#include <QByteArray>
#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <type_traits>

class ChildSessionController;
class ScreenStreamSender;

namespace E2eOracle {

// --- C++17 SFINAE M3 Seam Detectors ---
namespace detail {

template <typename T, typename = void>
struct HasSetFrameSource : std::false_type {};

template <typename T>
struct HasSetFrameSource<
    T,
    std::void_t<decltype(std::declval<T &>().setFrameSource(
        std::declval<std::function<QImage()>>()))>> : std::true_type {};

template <typename T, typename = void>
struct HasSetCpuUsageSource : std::false_type {};

template <typename T>
struct HasSetCpuUsageSource<
    T,
    std::void_t<decltype(std::declval<T &>().setCpuUsageSource(
        std::declval<std::function<double()>>()))>> : std::true_type {};

template <typename T, typename = void>
struct HasM3AckTelemetrySignals : std::false_type {};

template <typename T>
struct HasM3AckTelemetrySignals<
    T,
    std::void_t<decltype(&T::screenFrameAcknowledged),
                decltype(&T::screenFrameAckStalled)>> : std::true_type {};

} // namespace detail

bool trySetFrameSource(ScreenStreamSender *sender, std::function<QImage()> source);
bool trySetCpuUsageSource(ScreenStreamSender *sender, std::function<double()> source);
bool hasM3AckTelemetrySignals(const ChildSessionController *ctrl);

// --- 5 Deterministic Synthetic Frame Generators ---

class StaticTextFrameGenerator
{
public:
    explicit StaticTextFrameGenerator(const QSize &size = QSize(1920, 1080),
                                      bool cursorBlink = true);

    QImage nextFrame();
    void reset();
    QSize frameSize() const { return m_size; }
    int frameIndex() const { return m_frameIndex; }

private:
    void buildBaseCanvas();

    QSize m_size;
    bool m_cursorBlink = true;
    int m_frameIndex = 0;
    QImage m_baseCanvas;
};

class ScrollingUiFrameGenerator
{
public:
    explicit ScrollingUiFrameGenerator(const QSize &size = QSize(1920, 1080),
                                       int stepPx = 24);

    QImage nextFrame();
    void reset();
    void setStepPx(int stepPx);
    int stepPx() const { return m_stepPx; }
    QSize frameSize() const { return m_size; }
    int frameIndex() const { return m_frameIndex; }

private:
    void buildDocumentStrip();

    QSize m_size;
    int m_stepPx = 24;
    int m_frameIndex = 0;
    int m_scrollOffsetPx = 0;
    QImage m_documentStrip;
};

class HighMotionVideoFrameGenerator
{
public:
    explicit HighMotionVideoFrameGenerator(const QSize &size = QSize(1920, 1080));

    QImage nextFrame();
    void reset();
    QSize frameSize() const { return m_size; }
    int frameIndex() const { return m_frameIndex; }

private:
    QSize m_size;
    int m_frameIndex = 0;
};

class WorstCaseEntropyNoiseGenerator
{
public:
    explicit WorstCaseEntropyNoiseGenerator(const QSize &size = QSize(1920, 1080),
                                            quint32 seed = 42u);

    QImage nextFrame();
    void reset();
    void reset(quint32 seed);
    quint32 seed() const { return m_seed; }
    QSize frameSize() const { return m_size; }
    int frameIndex() const { return m_frameIndex; }

private:
    QSize m_size;
    quint32 m_seed = 42u;
    int m_frameIndex = 0;
};

class MarkerColorStepGenerator
{
public:
    explicit MarkerColorStepGenerator(const QSize &size = QSize(1280, 800));

    void setStep(int step);
    int step() const { return m_step; }
    QImage nextFrame();
    void reset();
    QSize frameSize() const { return m_size; }

    // Decodes quadrant colors from a received/decoded QImage back to step (0..255), or -1 if invalid.
    static int detectMarkerStep(const QImage &decoded);
    static QColor paletteColor(int index);

private:
    QSize m_size;
    int m_step = 0;
};

// --- Quality & Fidelity Oracles ---

double computePsnr(const QImage &reference, const QImage &decoded);
int estimateJpegQualityFromDqt(const QByteArray &jpegData);

// --- Rate-Limited StreamMetrics Log Collector ---

struct ParsedMetricsEntry
{
    qint64 timestampMs = 0;
    QString rawLine;

    double estimatedBandwidthBps = 0.0;
    double observedThroughputBps = 0.0;
    qsizetype frameByteBudget = 0;
    qsizetype actualFrameBytes = 0;
    double motionScore = 0.0;
    int jpegQuality = 0;
    QSize resolution;
    int targetFps = 0;
    double actualFps = 0.0;
    qint64 encodeTimeMs = 0;
    int encodeRetryCount = 0;
    double ackLatencyMs = -1.0;
    qint64 pendingBytes = 0;
    qint64 droppedFrames = 0;
    QString configChangeReason;

    bool all15FieldsPresent = false;
    bool allFieldsValid = false;
};

class StreamMetricsLogCollector
{
public:
    explicit StreamMetricsLogCollector(bool forwardToPrevious = true);
    ~StreamMetricsLogCollector();

    StreamMetricsLogCollector(const StreamMetricsLogCollector &) = delete;
    StreamMetricsLogCollector &operator=(const StreamMetricsLogCollector &) = delete;

    void clear();
    QList<ParsedMetricsEntry> entries() const;
    QStringList allCapturedLines() const;
    int count() const;

    static ParsedMetricsEntry parseLine(const QString &line, qint64 timestampMs = 0);

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    void recordMessage(const QString &msg);

    bool m_forwardToPrevious = true;
    QtMessageHandler m_previousHandler = nullptr;
    StreamMetricsLogCollector *m_previousCollector = nullptr;
    QElapsedTimer m_elapsed;
    mutable QMutex m_dataMutex;
    QList<ParsedMetricsEntry> m_entries;
    QStringList m_allLines;

    static QMutex s_handlerMutex;
    static StreamMetricsLogCollector *s_activeCollector;
};

// --- GUI Thread Responsiveness Probe (10 ms QTimer) ---

class GuiResponsivenessProbe : public QObject
{
    Q_OBJECT
public:
    explicit GuiResponsivenessProbe(int intervalMs = 10, QObject *parent = nullptr);
    ~GuiResponsivenessProbe() override;

    void start();
    void stop();
    void reset();

    int intervalMs() const { return m_intervalMs; }
    int tickCount() const { return m_delaysMs.size(); }
    qint64 maxDelayMs() const;
    double p95DelayMs() const;
    double meanDelayMs() const;
    QList<qint64> delaysMs() const { return m_delaysMs; }

private slots:
    void onTick();

private:
    int m_intervalMs = 10;
    QTimer m_timer;
    QElapsedTimer m_tickClock;
    QList<qint64> m_delaysMs;
};

} // namespace E2eOracle
