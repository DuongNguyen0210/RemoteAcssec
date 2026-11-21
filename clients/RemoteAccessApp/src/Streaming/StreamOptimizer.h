#pragma once

#include "Network/Protocol/ProtocolConstants.h"
#include "Network/Protocol/ScreenFramePacketizer.h"
#include "Streaming/ScreenEncoder.h"

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QSize>
#include <QVector>
#include <QtGlobal>

#include <deque>
#include <functional>

struct StreamConfig
{
    int quality = ScreenEncoder::DEFAULT_QUALITY;
    double scale = 1.0;
    int fps = 5;
    double objectiveScore = 0.0;
    qsizetype predictedBytes = 0;
};

struct EncodeAttemptResult
{
    QByteArray jpeg;
    QSize encodedSize;
    int qualityUsed = 0;
    double scaleUsed = 1.0;
    qsizetype wireBytes = 0;
    int encodeAttempts = 0;
    qint64 encodeTimeMs = 0;
    bool valid = false;
};

class StreamOptimizer
{
public:
    static constexpr qsizetype WIRE_OVERHEAD_BYTES =
        static_cast<qsizetype>(Protocol::HEADER_SIZE) + ScreenFramePacketizer::SCREEN_FRAME_METADATA_SIZE;
    static constexpr qsizetype DEFAULT_MAX_WIRE_FRAME_BYTES = 256 * 1024;

    static_assert(WIRE_OVERHEAD_BYTES == 40,
                  "RDTP v2 SCREEN_FRAME wire overhead must equal 40 bytes (24B header + 16B metadata)");
    static_assert(DEFAULT_MAX_WIRE_FRAME_BYTES - WIRE_OVERHEAD_BYTES <= ScreenFramePacketizer::maxChunkData(),
                  "Default single-packet Relay reservation payload must fit ScreenFramePacketizer::maxChunkData()");
    static_assert(DEFAULT_MAX_WIRE_FRAME_BYTES - WIRE_OVERHEAD_BYTES == 262104,
                  "Default effective max JPEG payload must be 262,104 bytes");

    struct Config
    {
        QVector<double> scaleRatios{1.0, 0.85, 0.75, 0.67, 0.5};
        QVector<int> fpsLevels{2, 3, 5, 8, 10, 15};
        int minQuality = 30;
        int maxQuality = 90;
        qsizetype maxWireFrameBytes = DEFAULT_MAX_WIRE_FRAME_BYTES;
        qsizetype headerOverheadBytes = WIRE_OVERHEAD_BYTES;
        double processingMarginMs = 8.0;
        double highRttFallbackMaxTxMs = 400.0;
        int maxResizeCacheEntries = 16;
        int maxEncodeCacheEntries = 128;
    };

    struct OptimizerContext
    {
        double availableBandwidthBps = 100000.0;
        double estimatedBandwidthBps = 125000.0;
        double smoothedRttMs = 0.0;
        double minRttMs = 0.0;
        double estimatedEncodeTimeMs = 0.0;
        qsizetype priorWireBytes = 0;
        double wQ = 0.35;
        double wR = 0.50;
        double wP = 0.15;
        double lowMotionScaleBias = 0.0;
        int minQuality = 30;
        int maxQuality = 90;
        double minScale = 0.5;
        double maxScale = 1.0;
        int minFps = 2;
        int maxFps = 15;
        qsizetype minRecoveryWireBytes = 0;
        bool allowHighRttFallback = true;
        int warmStartQuality = -1;
        double warmStartScale = -1.0;
        int warmStartFps = -1;
    };

    struct FastContext
    {
        qsizetype maxWireBytes = 0;
        qsizetype maxJpegBytes = 0;
        int minQuality = 30;
        int maxQuality = 90;
        int maxRetries = 3;
        qint64 maxTimeMs = 35;
    };

    struct OptimizeResult
    {
        StreamConfig bestConfig;
        EncodeAttemptResult encoded;
        qsizetype frameByteBudget = 0;
        qsizetype jpegByteBudget = 0;
        int totalEncodeCalls = 0;
        int resizeCalls = 0;
        int encodeCacheHits = 0;
        bool feasible = false;
        bool shouldDropFrame = false;
    };

    using EncodeFunction = std::function<TimedEncodedFrame(const QImage &scaledImage, int quality)>;

    StreamOptimizer();
    explicit StreamOptimizer(const Config &config);

    void setConfig(const Config &config);
    Config config() const;

    void setEncodeFunction(EncodeFunction fn);
    void clearCaches();

    qsizetype effectiveMaxPayload() const;
    qsizetype effectiveMaxWireBytes() const;

    double extractBaseRttMs(const OptimizerContext &ctx) const;
    qsizetype computeMaxWireBytes(int fps, const OptimizerContext &ctx) const;
    qsizetype computeMaxBytes(int fps, const OptimizerContext &ctx) const;
    int maxFeasibleFpsForWireBytes(qsizetype actualWireBytes, const OptimizerContext &ctx) const;

    double computeObjectiveScore(int quality,
                                 double scale,
                                 int fps,
                                 const OptimizerContext &ctx) const;

    OptimizeResult optimizeFull(quint64 frameId,
                                const QImage &frame,
                                const OptimizerContext &ctx);

    EncodeAttemptResult encodeFast(quint64 frameId,
                                   const QImage &frame,
                                   const StreamConfig &active,
                                   const FastContext &ctx);

    EncodeAttemptResult encodeExact(quint64 frameId,
                                    const QImage &frame,
                                    double scale,
                                    int quality,
                                    qsizetype maxJpegBytes);

private:
    struct ResizeCacheKey
    {
        quint64 frameId = 0;
        int scaleIndex = 0;

        bool operator==(const ResizeCacheKey &other) const
        {
            return frameId == other.frameId && scaleIndex == other.scaleIndex;
        }
    };

    struct ResizeCacheEntry
    {
        QImage image;
        qint64 sourceCacheKey = 0;
        QSize sourceSize;
    };

    struct EncodeCacheKey
    {
        quint64 frameId = 0;
        int scaleIndex = 0;
        int quality = 0;

        bool operator==(const EncodeCacheKey &other) const
        {
            return frameId == other.frameId
                && scaleIndex == other.scaleIndex
                && quality == other.quality;
        }
    };

    struct EncodeCacheEntry
    {
        TimedEncodedFrame frame;
        qint64 sourceCacheKey = 0;
        QSize sourceSize;
    };

    static Config sanitizeConfig(const Config &cfg);
    static void normalizeWeights(double wQ,
                                 double wR,
                                 double wP,
                                 double &outWQ,
                                 double &outWR,
                                 double &outWP);

    int resolveScaleIndex(double scale) const;
    QImage getOrResizeImage(quint64 frameId,
                            const QImage &source,
                            int scaleIndex,
                            double scale,
                            int &resizeCalls);
    TimedEncodedFrame getOrEncodeFrame(quint64 frameId,
                                       const QImage &source,
                                       int scaleIndex,
                                       double scale,
                                       int quality,
                                       int &resizeCalls,
                                       int &encodeCalls,
                                       int &cacheHits);

    Config m_config;
    EncodeFunction m_encodeFn;

    QHash<ResizeCacheKey, ResizeCacheEntry> m_resizeCache;
    std::deque<ResizeCacheKey> m_resizeOrder;

    QHash<EncodeCacheKey, EncodeCacheEntry> m_encodeCache;
    std::deque<EncodeCacheKey> m_encodeOrder;

    friend size_t qHash(const ResizeCacheKey &key, size_t seed) noexcept;
    friend size_t qHash(const EncodeCacheKey &key, size_t seed) noexcept;
};

inline size_t qHash(const StreamOptimizer::ResizeCacheKey &key, size_t seed = 0) noexcept
{
    return qHashMulti(seed, key.frameId, key.scaleIndex);
}

inline size_t qHash(const StreamOptimizer::EncodeCacheKey &key, size_t seed = 0) noexcept
{
    return qHashMulti(seed, key.frameId, key.scaleIndex, key.quality);
}
