#include "E2eFrameOracle.h"

#include "Network/Relay/ChildSessionController.h"
#include "Streaming/ScreenStreamSender.h"

#include <QFont>
#include <QMutexLocker>
#include <QPainter>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace E2eOracle {

namespace {

constexpr std::array<int, 64> kAnnexKLuminanceZigZag = {
    16,  11,  12,  14,  12,  10,  16,  14,
    13,  14,  18,  17,  16,  19,  24,  40,
    26,  24,  22,  22,  24,  49,  35,  37,
    29,  40,  58,  51,  61,  60,  57,  51,
    56,  55,  64,  72,  92,  78,  64,  68,
    87,  69,  55,  56,  80, 109,  81,  87,
    95,  98, 103, 104, 103,  62,  77, 113,
   121, 112, 100, 120,  92, 101, 103,  99
};

template <typename SenderT>
bool trySetFrameSourceImpl(SenderT *sender, std::function<QImage()> source)
{
    if (!sender)
        return false;
    if constexpr (detail::HasSetFrameSource<SenderT>::value) {
        sender->setFrameSource(std::move(source));
        return true;
    } else {
        Q_UNUSED(source);
        return false;
    }
}

template <typename SenderT>
bool trySetCpuUsageSourceImpl(SenderT *sender, std::function<double()> source)
{
    if (!sender)
        return false;
    if constexpr (detail::HasSetCpuUsageSource<SenderT>::value) {
        sender->setCpuUsageSource(std::move(source));
        return true;
    } else {
        Q_UNUSED(source);
        return false;
    }
}

} // namespace

// --- SFINAE M3 Seam Adapters ---

bool trySetFrameSource(ScreenStreamSender *sender, std::function<QImage()> source)
{
    return trySetFrameSourceImpl(sender, std::move(source));
}

bool trySetCpuUsageSource(ScreenStreamSender *sender, std::function<double()> source)
{
    return trySetCpuUsageSourceImpl(sender, std::move(source));
}

bool hasM3AckTelemetrySignals(const ChildSessionController *ctrl)
{
    if (!ctrl)
        return false;
    return detail::HasM3AckTelemetrySignals<ChildSessionController>::value;
}

// --- StaticTextFrameGenerator ---

StaticTextFrameGenerator::StaticTextFrameGenerator(const QSize &size, bool cursorBlink)
    : m_size(size)
    , m_cursorBlink(cursorBlink)
{
    buildBaseCanvas();
}

void StaticTextFrameGenerator::reset()
{
    m_frameIndex = 0;
}

void StaticTextFrameGenerator::buildBaseCanvas()
{
    if (!m_size.isValid() || m_size.isEmpty()) {
        m_baseCanvas = QImage();
        return;
    }

    m_baseCanvas = QImage(m_size, QImage::Format_RGB32);
    m_baseCanvas.fill(QColor(30, 30, 46)); // #1E1E2E

    QPainter painter(&m_baseCanvas);
    painter.setRenderHint(QPainter::TextAntialiasing, false);

    // Line-number gutter
    const int gutterWidth = qMin(68, m_size.width() / 6);
    painter.fillRect(0, 0, gutterWidth, m_size.height(), QColor(42, 42, 61)); // #2A2A3D

    // Header bar
    painter.fillRect(0, 0, m_size.width(), 28, QColor(24, 24, 37));
    painter.setPen(QColor(205, 214, 244));

    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::Monospace);
    mono.setPixelSize(14);
    mono.setHintingPreference(QFont::PreferFullHinting);
    painter.setFont(mono);

    painter.drawText(16, 20, QStringLiteral("AdaptiveStreamController.cpp - RemoteAccessApp [UTF-8]"));

    static const QStringList kCodeSnippets = {
        QStringLiteral("constexpr double kSafetyFactor = 0.80; // target headroom"),
        QStringLiteral("StreamConfig optimize(const FrameStats &stats, double budgetBps) {"),
        QStringLiteral("    const double wQ = 0.35 - 0.15 * stats.motionScore;"),
        QStringLiteral("    const double wR = 0.50 - 0.25 * stats.motionScore;"),
        QStringLiteral("    const double wP = 0.15 + 0.40 * stats.motionScore;"),
        QStringLiteral("    if (stats.pendingBytes > kBackpressureThreshold) return fallback();"),
        QStringLiteral("    return binarySearchQuality(frame, budgetBytes, wQ, wR, wP);"),
        QStringLiteral("}")
    };

    const QColor palette[] = {
        QColor(137, 180, 250), // #89B4FA
        QColor(166, 227, 161), // #A6E3A1
        QColor(249, 226, 175), // #F9E2AF
        QColor(205, 214, 244)  // #CDD6F4
    };

    const int rowPitch = 22;
    const int startY = 50;
    const int textStartX = gutterWidth + 14;
    int lineNum = 1;
    for (int y = startY; y < m_size.height() - 12; y += rowPitch, ++lineNum) {
        painter.setPen(QColor(127, 132, 156));
        painter.drawText(8, y, QString::number(lineNum).rightJustified(4, QLatin1Char(' ')));

        const QString &snippet = kCodeSnippets.at((lineNum - 1) % kCodeSnippets.size());
        const QString fullLine = QStringLiteral("%1  /* line=%2 */ %3")
                                     .arg(snippet)
                                     .arg(lineNum)
                                     .arg(QStringLiteral("validatePayload(frameBytes <= 262104);"));
        painter.setPen(palette[lineNum % 4]);
        painter.drawText(textStartX, y, fullLine);
    }
}

QImage StaticTextFrameGenerator::nextFrame()
{
    if (m_baseCanvas.isNull())
        return QImage();

    QImage frame = m_baseCanvas.copy();
    if (m_cursorBlink && (m_frameIndex % 2 == 1)) {
        QPainter painter(&frame);
        const int cx = qBound(0, 240, qMax(0, m_size.width() - 3));
        const int cy = qBound(0, 180, qMax(0, m_size.height() - 16));
        painter.fillRect(cx, cy, qMin(3, m_size.width()), qMin(16, m_size.height()),
                         QColor(245, 224, 220)); // #F5E0DC
    }
    ++m_frameIndex;
    return frame;
}

// --- ScrollingUiFrameGenerator ---

ScrollingUiFrameGenerator::ScrollingUiFrameGenerator(const QSize &size, int stepPx)
    : m_size(size)
    , m_stepPx(qMax(1, stepPx))
{
    buildDocumentStrip();
}

void ScrollingUiFrameGenerator::setStepPx(int stepPx)
{
    m_stepPx = qMax(1, stepPx);
}

void ScrollingUiFrameGenerator::reset()
{
    m_frameIndex = 0;
    m_scrollOffsetPx = 0;
}

void ScrollingUiFrameGenerator::buildDocumentStrip()
{
    if (!m_size.isValid() || m_size.isEmpty()) {
        m_documentStrip = QImage();
        return;
    }

    const int stripH = m_size.height() * 2;
    m_documentStrip = QImage(m_size.width(), stripH, QImage::Format_RGB32);
    m_documentStrip.fill(QColor(248, 250, 252)); // #F8FAFC

    QPainter painter(&m_documentStrip);
    QFont font(QStringLiteral("Monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(15);
    painter.setFont(font);

    const int rowPitch = 36;
    const int marginX = 48;
    const int cardW = qMax(64, m_size.width() - 2 * marginX);

    int idx = 0;
    for (int y = 12; y < stripH - 28; y += rowPitch, ++idx) {
        const QColor cardBg = (idx % 2 == 0) ? QColor(255, 255, 255) : QColor(226, 232, 240);
        painter.fillRect(marginX, y, cardW, rowPitch - 6, cardBg);

        const QColor accent((idx * 53) % 200 + 20, (idx * 97) % 180 + 30, (idx * 131) % 200 + 40);
        painter.fillRect(marginX, y, 8, rowPitch - 6, accent);

        painter.setPen(QColor(15, 23, 42));
        painter.drawText(marginX + 20, y + 20,
                         QStringLiteral("Row #%1  | Session telemetry event: throughput=%2 kbps  status=OK")
                             .arg(idx, 4, 10, QLatin1Char('0'))
                             .arg(400 + (idx * 75) % 4000));

        const int barW = qMax(20, ((idx * 37) % 100) * (cardW / 4) / 100);
        painter.fillRect(marginX + cardW - barW - 16, y + 8, barW, 12, accent);

        painter.setPen(QColor(203, 213, 225));
        painter.drawLine(marginX, y + rowPitch - 4, marginX + cardW, y + rowPitch - 4);
    }
}

QImage ScrollingUiFrameGenerator::nextFrame()
{
    if (m_documentStrip.isNull() || m_size.isEmpty())
        return QImage();

    const int w = m_size.width();
    const int h = m_size.height();
    const int stripH = m_documentStrip.height();
    const int offset = m_scrollOffsetPx % stripH;

    QImage out;
    if (offset + h <= stripH) {
        out = m_documentStrip.copy(0, offset, w, h);
    } else {
        out = QImage(w, h, QImage::Format_RGB32);
        const int firstH = stripH - offset;
        const int secondH = h - firstH;
        QPainter painter(&out);
        painter.drawImage(0, 0, m_documentStrip, 0, offset, w, firstH);
        painter.drawImage(0, firstH, m_documentStrip, 0, 0, w, secondH);
    }

    m_scrollOffsetPx = (m_scrollOffsetPx + m_stepPx) % stripH;
    ++m_frameIndex;
    return out;
}

// --- HighMotionVideoFrameGenerator ---

HighMotionVideoFrameGenerator::HighMotionVideoFrameGenerator(const QSize &size)
    : m_size(size)
{
}

void HighMotionVideoFrameGenerator::reset()
{
    m_frameIndex = 0;
}

QImage HighMotionVideoFrameGenerator::nextFrame()
{
    if (!m_size.isValid() || m_size.isEmpty())
        return QImage();

    QImage frame(m_size, QImage::Format_RGB32);
    QPainter painter(&frame);

    const int tile = 128;
    const int cols = (m_size.width() + tile - 1) / tile;
    const int rows = (m_size.height() + tile - 1) / tile;

    for (int by = 0; by < rows; ++by) {
        for (int bx = 0; bx < cols; ++bx) {
            const int r = ((bx * 47 + by * 71 + m_frameIndex * 113) % 256);
            const int g = ((bx * 83 + by * 29 + m_frameIndex * 157) % 256);
            const int b = ((bx * 19 + by * 103 + m_frameIndex * 79) % 256);
            painter.fillRect(bx * tile, by * tile, tile, tile, QColor(r, g, b));
        }
    }

    // Moving high-contrast diagonal bands for large-area smooth motion
    const int stripePeriod = 192;
    const int shift = (m_frameIndex * 67) % stripePeriod;
    painter.setPen(Qt::NoPen);
    for (int i = -m_size.height(); i < m_size.width() + m_size.height(); i += stripePeriod) {
        QPolygon poly;
        const int x0 = i + shift;
        poly << QPoint(x0, 0)
             << QPoint(x0 + 48, 0)
             << QPoint(x0 + 48 - m_size.height() / 2, m_size.height())
             << QPoint(x0 - m_size.height() / 2, m_size.height());
        painter.setBrush(QColor((m_frameIndex * 91 + 40) % 256,
                                (m_frameIndex * 149 + 80) % 256,
                                (m_frameIndex * 211 + 120) % 256,
                                180));
        painter.drawPolygon(poly);
    }

    ++m_frameIndex;
    return frame;
}

// --- WorstCaseEntropyNoiseGenerator ---

WorstCaseEntropyNoiseGenerator::WorstCaseEntropyNoiseGenerator(const QSize &size, quint32 seed)
    : m_size(size)
    , m_seed(seed)
{
}

void WorstCaseEntropyNoiseGenerator::reset()
{
    m_frameIndex = 0;
}

void WorstCaseEntropyNoiseGenerator::reset(quint32 seed)
{
    m_seed = seed;
    m_frameIndex = 0;
}

QImage WorstCaseEntropyNoiseGenerator::nextFrame()
{
    if (!m_size.isValid() || m_size.isEmpty())
        return QImage();

    QImage frame(m_size, QImage::Format_RGB32);
    quint32 state = m_seed ^ (static_cast<quint32>(m_frameIndex) * 0x9E3779B9u) ^ 0xA5A55A5Au;
    if (state == 0)
        state = 1u;

    const int w = m_size.width();
    const int h = m_size.height();
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(frame.scanLine(y));
        for (int x = 0; x < w; ++x) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            line[x] = 0xFF000000u | (state & 0x00FFFFFFu);
        }
    }

    ++m_frameIndex;
    return frame;
}

// --- MarkerColorStepGenerator ---

MarkerColorStepGenerator::MarkerColorStepGenerator(const QSize &size)
    : m_size(size)
{
}

void MarkerColorStepGenerator::setStep(int step)
{
    m_step = step & 0xFF;
}

void MarkerColorStepGenerator::reset()
{
    m_step = 0;
}

QColor MarkerColorStepGenerator::paletteColor(int index)
{
    switch (index & 0x3) {
    case 0: return QColor(220, 30, 30);   // Red
    case 1: return QColor(30, 210, 40);   // Green
    case 2: return QColor(30, 50, 225);   // Blue
    default: return QColor(230, 220, 30); // Yellow
    }
}

QImage MarkerColorStepGenerator::nextFrame()
{
    if (!m_size.isValid() || m_size.isEmpty())
        return QImage();

    QImage frame(m_size, QImage::Format_RGB32);
    QPainter painter(&frame);

    const int halfW = m_size.width() / 2;
    const int halfH = m_size.height() / 2;

    painter.fillRect(0, 0, halfW, halfH, paletteColor((m_step >> 0) & 0x3));
    painter.fillRect(halfW, 0, m_size.width() - halfW, halfH, paletteColor((m_step >> 2) & 0x3));
    painter.fillRect(0, halfH, halfW, m_size.height() - halfH, paletteColor((m_step >> 4) & 0x3));
    painter.fillRect(halfW, halfH, m_size.width() - halfW, m_size.height() - halfH,
                     paletteColor((m_step >> 6) & 0x3));

    return frame;
}

int MarkerColorStepGenerator::detectMarkerStep(const QImage &decoded)
{
    if (decoded.isNull() || decoded.width() < 8 || decoded.height() < 8)
        return -1;

    const QImage rgb = (decoded.format() == QImage::Format_RGB32
                        || decoded.format() == QImage::Format_ARGB32)
        ? decoded
        : decoded.convertToFormat(QImage::Format_RGB32);

    const int w = rgb.width();
    const int h = rgb.height();

    const std::array<QPoint, 4> centers = {
        QPoint(w / 4, h / 4),
        QPoint((3 * w) / 4, h / 4),
        QPoint(w / 4, (3 * h) / 4),
        QPoint((3 * w) / 4, (3 * h) / 4)
    };

    int resultStep = 0;
    for (int q = 0; q < 4; ++q) {
        const int cx = centers[q].x();
        const int cy = centers[q].y();
        long sumR = 0;
        long sumG = 0;
        long sumB = 0;
        int samples = 0;

        for (int dy = -2; dy <= 2; ++dy) {
            const int py = qBound(0, cy + dy, h - 1);
            const auto *line = reinterpret_cast<const QRgb *>(rgb.constScanLine(py));
            for (int dx = -2; dx <= 2; ++dx) {
                const int px = qBound(0, cx + dx, w - 1);
                const QRgb pxVal = line[px];
                sumR += qRed(pxVal);
                sumG += qGreen(pxVal);
                sumB += qBlue(pxVal);
                ++samples;
            }
        }

        const int meanR = static_cast<int>(sumR / samples);
        const int meanG = static_cast<int>(sumG / samples);
        const int meanB = static_cast<int>(sumB / samples);

        int bestIdx = 0;
        long bestDistSq = std::numeric_limits<long>::max();
        for (int p = 0; p < 4; ++p) {
            const QColor c = paletteColor(p);
            const long dr = meanR - c.red();
            const long dg = meanG - c.green();
            const long db = meanB - c.blue();
            const long distSq = dr * dr + dg * dg + db * db;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestIdx = p;
            }
        }

        resultStep |= (bestIdx << (q * 2));
    }

    return resultStep;
}

// --- Quality & Fidelity Oracles ---

double computePsnr(const QImage &reference, const QImage &decoded)
{
    if (reference.isNull() || reference.size().isEmpty()
        || decoded.isNull() || decoded.size().isEmpty()) {
        return 0.0;
    }

    QImage scaledDecoded = (decoded.size() == reference.size())
        ? decoded
        : decoded.scaled(reference.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    const QImage refRgb = (reference.format() == QImage::Format_RGB32
                           || reference.format() == QImage::Format_ARGB32)
        ? reference
        : reference.convertToFormat(QImage::Format_RGB32);
    const QImage decRgb = (scaledDecoded.format() == QImage::Format_RGB32
                           || scaledDecoded.format() == QImage::Format_ARGB32)
        ? scaledDecoded
        : scaledDecoded.convertToFormat(QImage::Format_RGB32);

    const int w = refRgb.width();
    const int h = refRgb.height();
    double sumSq = 0.0;

    for (int y = 0; y < h; ++y) {
        const auto *refLine = reinterpret_cast<const QRgb *>(refRgb.constScanLine(y));
        const auto *decLine = reinterpret_cast<const QRgb *>(decRgb.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const double dr = static_cast<double>(qRed(refLine[x])) - static_cast<double>(qRed(decLine[x]));
            const double dg = static_cast<double>(qGreen(refLine[x])) - static_cast<double>(qGreen(decLine[x]));
            const double db = static_cast<double>(qBlue(refLine[x])) - static_cast<double>(qBlue(decLine[x]));
            sumSq += dr * dr + dg * dg + db * db;
        }
    }

    const double mse = sumSq / (3.0 * static_cast<double>(w) * static_cast<double>(h));
    if (mse <= 1e-12)
        return 100.0;

    return 10.0 * std::log10((255.0 * 255.0) / mse);
}

int estimateJpegQualityFromDqt(const QByteArray &jpegData)
{
    const auto *bytes = reinterpret_cast<const uchar *>(jpegData.constData());
    const int n = static_cast<int>(jpegData.size());
    if (n < 4 || bytes[0] != 0xFF || bytes[1] != 0xD8)
        return -1;

    std::array<int, 64> lumaTable{};
    bool foundLuma = false;

    int i = 2;
    while (i + 1 < n) {
        if (bytes[i] != 0xFF) {
            ++i;
            continue;
        }
        while (i < n && bytes[i] == 0xFF)
            ++i;
        if (i >= n)
            break;

        const uchar marker = bytes[i++];
        if (marker == 0xD9 || marker == 0xDA) // EOI or SOS
            break;
        if (marker == 0x00 || marker == 0x01 || marker == 0xD8
            || (marker >= 0xD0 && marker <= 0xD7)) {
            continue;
        }

        if (i + 1 >= n)
            break;
        const int segLen = (static_cast<int>(bytes[i]) << 8) | static_cast<int>(bytes[i + 1]);
        if (segLen < 2 || i + segLen > n)
            break;

        if (marker == 0xDB) { // DQT
            int pos = i + 2;
            const int end = i + segLen;
            while (pos < end) {
                const uchar pqTq = bytes[pos++];
                const int pq = (pqTq >> 4) & 0x0F;
                const int tq = pqTq & 0x0F;
                const int elemBytes = (pq == 0) ? 1 : (pq == 1 ? 2 : 0);
                if (elemBytes == 0 || pos + 64 * elemBytes > end)
                    return -1;

                if (tq == 0 && !foundLuma) {
                    for (int k = 0; k < 64; ++k) {
                        if (elemBytes == 1) {
                            lumaTable[k] = bytes[pos + k];
                        } else {
                            lumaTable[k] = (static_cast<int>(bytes[pos + 2 * k]) << 8)
                                         | static_cast<int>(bytes[pos + 2 * k + 1]);
                        }
                    }
                    foundLuma = true;
                }
                pos += 64 * elemBytes;
            }
        }

        i += segLen;
    }

    if (!foundLuma)
        return -1;

    int bestQ = -1;
    double bestError = std::numeric_limits<double>::max();
    for (int q = 1; q <= 100; ++q) {
        const int scale = (q < 50) ? (5000 / q) : (200 - 2 * q);
        double err = 0.0;
        for (int k = 0; k < 64; ++k) {
            int expected = (kAnnexKLuminanceZigZag[k] * scale + 50) / 100;
            expected = qBound(1, expected, 255);
            const double diff = static_cast<double>(lumaTable[k] - expected);
            err += diff * diff;
        }
        if (err < bestError) {
            bestError = err;
            bestQ = q;
        }
    }

    return bestQ;
}

// --- StreamMetricsLogCollector ---

QMutex StreamMetricsLogCollector::s_handlerMutex;
StreamMetricsLogCollector *StreamMetricsLogCollector::s_activeCollector = nullptr;

StreamMetricsLogCollector::StreamMetricsLogCollector(bool forwardToPrevious)
    : m_forwardToPrevious(forwardToPrevious)
{
    m_elapsed.start();
    QMutexLocker lock(&s_handlerMutex);
    m_previousCollector = s_activeCollector;
    const QtMessageHandler prev = qInstallMessageHandler(&StreamMetricsLogCollector::messageHandler);
    m_previousHandler = (prev == &StreamMetricsLogCollector::messageHandler)
        ? (m_previousCollector ? m_previousCollector->m_previousHandler : nullptr)
        : prev;
    s_activeCollector = this;
}

StreamMetricsLogCollector::~StreamMetricsLogCollector()
{
    QMutexLocker lock(&s_handlerMutex);
    if (s_activeCollector == this) {
        s_activeCollector = m_previousCollector;
        if (!s_activeCollector)
            qInstallMessageHandler(m_previousHandler);
    } else {
        StreamMetricsLogCollector *curr = s_activeCollector;
        while (curr) {
            if (curr->m_previousCollector == this) {
                curr->m_previousCollector = m_previousCollector;
                break;
            }
            curr = curr->m_previousCollector;
        }
    }
}

void StreamMetricsLogCollector::messageHandler(QtMsgType type,
                                               const QMessageLogContext &context,
                                               const QString &msg)
{
    QtMessageHandler rootHandler = nullptr;
    bool shouldForward = true;
    {
        QMutexLocker lock(&s_handlerMutex);
        for (StreamMetricsLogCollector *c = s_activeCollector; c != nullptr; c = c->m_previousCollector) {
            c->recordMessage(msg);
            if (!c->m_previousCollector)
                rootHandler = c->m_previousHandler;
            if (!c->m_forwardToPrevious) {
                shouldForward = false;
                break;
            }
        }
    }
    if (shouldForward && rootHandler)
        rootHandler(type, context, msg);
}

void StreamMetricsLogCollector::recordMessage(const QString &msg)
{
    const qint64 ts = m_elapsed.elapsed();
    QMutexLocker lock(&m_dataMutex);
    m_allLines.append(msg);
    if (msg.contains(QStringLiteral("[StreamMetrics] ")))
        m_entries.append(parseLine(msg, ts));
}

void StreamMetricsLogCollector::clear()
{
    QMutexLocker lock(&m_dataMutex);
    m_entries.clear();
    m_allLines.clear();
}

QList<ParsedMetricsEntry> StreamMetricsLogCollector::entries() const
{
    QMutexLocker lock(&m_dataMutex);
    return m_entries;
}

QStringList StreamMetricsLogCollector::allCapturedLines() const
{
    QMutexLocker lock(&m_dataMutex);
    return m_allLines;
}

int StreamMetricsLogCollector::count() const
{
    QMutexLocker lock(&m_dataMutex);
    return static_cast<int>(m_entries.size());
}

ParsedMetricsEntry StreamMetricsLogCollector::parseLine(const QString &line, qint64 timestampMs)
{
    ParsedMetricsEntry entry;
    entry.timestampMs = timestampMs;
    entry.rawLine = line;

    const int markerIdx = line.indexOf(QStringLiteral("[StreamMetrics] "));
    if (markerIdx < 0)
        return entry;

    const QString body = line.mid(markerIdx);

    static const QRegularExpression reEstBw(QStringLiteral(R"(estBwBps=([-+]?\d+(?:\.\d+)?))"));
    static const QRegularExpression reObsTp(QStringLiteral(R"(obsThroughputBps=([-+]?\d+(?:\.\d+)?))"));
    static const QRegularExpression reBudget(QStringLiteral(R"(byteBudget=(-?\d+))"));
    static const QRegularExpression reFrameBytes(QStringLiteral(R"(frameBytes=(-?\d+))"));
    static const QRegularExpression reMotion(QStringLiteral(R"(motion=([-+]?\d+(?:\.\d+)?))"));
    static const QRegularExpression reQuality(QStringLiteral(R"(quality=(-?\d+))"));
    static const QRegularExpression reRes(QStringLiteral(R"(res=(\d+)x(\d+))"));
    static const QRegularExpression reTargetFps(QStringLiteral(R"(targetFps=(-?\d+))"));
    static const QRegularExpression reActualFps(QStringLiteral(R"(actualFps=([-+]?\d+(?:\.\d+)?))"));
    static const QRegularExpression reEncodeMs(QStringLiteral(R"(encodeMs=(-?\d+))"));
    static const QRegularExpression reRetries(QStringLiteral(R"(retries=(-?\d+))"));
    static const QRegularExpression reAckRtt(QStringLiteral(R"(ackRttMs=([-+]?\d+(?:\.\d+)?))"));
    static const QRegularExpression rePending(QStringLiteral(R"(pendingBytes=(-?\d+))"));
    static const QRegularExpression reDropped(QStringLiteral(R"(dropped=(-?\d+))"));
    static const QRegularExpression reReason(QStringLiteral(R"(reason=\\?"([^"\\]*)\\?")"));

    const auto mEstBw = reEstBw.match(body);
    const auto mObsTp = reObsTp.match(body);
    const auto mBudget = reBudget.match(body);
    const auto mFrameBytes = reFrameBytes.match(body);
    const auto mMotion = reMotion.match(body);
    const auto mQuality = reQuality.match(body);
    const auto mRes = reRes.match(body);
    const auto mTargetFps = reTargetFps.match(body);
    const auto mActualFps = reActualFps.match(body);
    const auto mEncodeMs = reEncodeMs.match(body);
    const auto mRetries = reRetries.match(body);
    const auto mAckRtt = reAckRtt.match(body);
    const auto mPending = rePending.match(body);
    const auto mDropped = reDropped.match(body);
    const auto mReason = reReason.match(body);

    entry.all15FieldsPresent =
        mEstBw.hasMatch() && mObsTp.hasMatch() && mBudget.hasMatch() && mFrameBytes.hasMatch()
        && mMotion.hasMatch() && mQuality.hasMatch() && mRes.hasMatch() && mTargetFps.hasMatch()
        && mActualFps.hasMatch() && mEncodeMs.hasMatch() && mRetries.hasMatch() && mAckRtt.hasMatch()
        && mPending.hasMatch() && mDropped.hasMatch() && mReason.hasMatch();

    if (!entry.all15FieldsPresent)
        return entry;

    bool ok[15]{};
    entry.estimatedBandwidthBps = mEstBw.captured(1).toDouble(&ok[0]);
    entry.observedThroughputBps = mObsTp.captured(1).toDouble(&ok[1]);
    entry.frameByteBudget = static_cast<qsizetype>(mBudget.captured(1).toLongLong(&ok[2]));
    entry.actualFrameBytes = static_cast<qsizetype>(mFrameBytes.captured(1).toLongLong(&ok[3]));
    entry.motionScore = mMotion.captured(1).toDouble(&ok[4]);
    entry.jpegQuality = mQuality.captured(1).toInt(&ok[5]);
    const int resW = mRes.captured(1).toInt(&ok[6]);
    const int resH = mRes.captured(2).toInt(&ok[7]);
    entry.resolution = QSize(resW, resH);
    entry.targetFps = mTargetFps.captured(1).toInt(&ok[8]);
    entry.actualFps = mActualFps.captured(1).toDouble(&ok[9]);
    entry.encodeTimeMs = mEncodeMs.captured(1).toLongLong(&ok[10]);
    entry.encodeRetryCount = mRetries.captured(1).toInt(&ok[11]);
    entry.ackLatencyMs = mAckRtt.captured(1).toDouble(&ok[12]);
    entry.pendingBytes = mPending.captured(1).toLongLong(&ok[13]);
    entry.droppedFrames = mDropped.captured(1).toLongLong(&ok[14]);
    entry.configChangeReason = mReason.captured(1);

    const bool allConverted = std::all_of(std::begin(ok), std::end(ok), [](bool b) { return b; });
    const bool finiteValues = std::isfinite(entry.estimatedBandwidthBps)
        && std::isfinite(entry.observedThroughputBps)
        && std::isfinite(entry.motionScore)
        && std::isfinite(entry.actualFps)
        && std::isfinite(entry.ackLatencyMs);

    entry.allFieldsValid = allConverted && finiteValues
        && entry.estimatedBandwidthBps >= 0.0
        && entry.observedThroughputBps >= 0.0
        && entry.frameByteBudget >= 0
        && entry.actualFrameBytes >= 0
        && entry.motionScore >= 0.0 && entry.motionScore <= 1.0
        && entry.jpegQuality >= 0 && entry.jpegQuality <= 100
        && resW >= 0 && resH >= 0
        && entry.targetFps >= 0
        && entry.actualFps >= 0.0
        && entry.encodeTimeMs >= 0
        && entry.encodeRetryCount >= 0
        && (entry.ackLatencyMs == -1.0 || entry.ackLatencyMs >= 0.0)
        && entry.pendingBytes >= 0
        && entry.droppedFrames >= 0
        && !entry.configChangeReason.isEmpty();

    return entry;
}

// --- GuiResponsivenessProbe ---

GuiResponsivenessProbe::GuiResponsivenessProbe(int intervalMs, QObject *parent)
    : QObject(parent)
    , m_intervalMs(qMax(1, intervalMs))
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(m_intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &GuiResponsivenessProbe::onTick);
}

GuiResponsivenessProbe::~GuiResponsivenessProbe()
{
    stop();
}

void GuiResponsivenessProbe::start()
{
    m_delaysMs.clear();
    m_tickClock.restart();
    m_timer.start(m_intervalMs);
}

void GuiResponsivenessProbe::stop()
{
    m_timer.stop();
}

void GuiResponsivenessProbe::reset()
{
    m_delaysMs.clear();
    m_tickClock.restart();
}

void GuiResponsivenessProbe::onTick()
{
    const qint64 elapsed = m_tickClock.restart();
    m_delaysMs.append(qMax<qint64>(0, elapsed - m_intervalMs));
}

qint64 GuiResponsivenessProbe::maxDelayMs() const
{
    if (m_delaysMs.isEmpty())
        return 0;
    return *std::max_element(m_delaysMs.cbegin(), m_delaysMs.cend());
}

double GuiResponsivenessProbe::p95DelayMs() const
{
    if (m_delaysMs.isEmpty())
        return 0.0;
    QList<qint64> sorted = m_delaysMs;
    std::sort(sorted.begin(), sorted.end());
    const qsizetype idx = std::min<qsizetype>(
        sorted.size() - 1,
        qMax<qsizetype>(0, static_cast<qsizetype>(std::ceil(0.95 * static_cast<double>(sorted.size()))) - 1));
    return static_cast<double>(sorted.at(idx));
}

double GuiResponsivenessProbe::meanDelayMs() const
{
    if (m_delaysMs.isEmpty())
        return 0.0;
    qint64 sum = 0;
    for (qint64 d : m_delaysMs)
        sum += d;
    return static_cast<double>(sum) / static_cast<double>(m_delaysMs.size());
}

} // namespace E2eOracle
