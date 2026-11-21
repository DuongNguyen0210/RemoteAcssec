#pragma once

#include <QImage>
#include <QMutex>
#include <QSize>

struct MotionDetectorConfig
{
    QSize thumbnailSize{160, 90};
    double emaAlpha = 0.30;
    int pixelDiffThreshold = 8;

    double wQBase = 0.35;
    double wQSlope = 0.15;
    double wRBase = 0.50;
    double wRSlope = 0.25;
    double wPBase = 0.15;
    double wPSlope = 0.40;
};

struct MotionResult
{
    double rawScore = 0.0;
    double smoothedScore = 0.0;
    double wQ = 0.35;
    double wR = 0.50;
    double wP = 0.15;
    bool valid = false;
};

class MotionDetector
{
public:
    using Config = MotionDetectorConfig;

    MotionDetector();
    explicit MotionDetector(const Config &config);

    MotionResult analyze(const QImage &frame);
    void reset();

    void setConfig(const Config &config);
    Config config() const;
    double smoothedScore() const;

private:
    static QImage toGrayThumbnail(const QImage &frame, const QSize &targetSize);
    static void computeWeights(double motionScore,
                               const Config &cfg,
                               double &wQ,
                               double &wR,
                               double &wP);
    static Config sanitizeConfig(const Config &cfg);

    mutable QMutex m_mutex;
    Config m_config;
    QImage m_prevThumbnail;
    double m_smoothedScore = 0.0;
    bool m_hasPrevious = false;
};
