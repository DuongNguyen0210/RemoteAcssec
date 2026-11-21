#include "MotionDetector.h"

#include <QMutexLocker>

#include <algorithm>
#include <cmath>
#include <cstdlib>

MotionDetector::MotionDetector()
    : m_config(sanitizeConfig(Config{}))
{
}

MotionDetector::MotionDetector(const Config &config)
    : m_config(sanitizeConfig(config))
{
}

MotionDetector::Config MotionDetector::sanitizeConfig(const Config &cfg)
{
    Config out = cfg;
    if (!out.thumbnailSize.isValid() || out.thumbnailSize.width() <= 0 || out.thumbnailSize.height() <= 0)
    {
        out.thumbnailSize = QSize(160, 90);
    }

    out.emaAlpha = std::isfinite(out.emaAlpha) ? std::clamp(out.emaAlpha, 0.0, 1.0) : 0.30;
    out.pixelDiffThreshold = std::clamp(out.pixelDiffThreshold, 0, 255);

    out.wQBase = std::isfinite(out.wQBase) ? out.wQBase : 0.35;
    out.wQSlope = std::isfinite(out.wQSlope) ? out.wQSlope : 0.15;
    out.wRBase = std::isfinite(out.wRBase) ? out.wRBase : 0.50;
    out.wRSlope = std::isfinite(out.wRSlope) ? out.wRSlope : 0.25;
    out.wPBase = std::isfinite(out.wPBase) ? out.wPBase : 0.15;
    out.wPSlope = std::isfinite(out.wPSlope) ? out.wPSlope : 0.40;

    return out;
}

QImage MotionDetector::toGrayThumbnail(const QImage &frame, const QSize &targetSize)
{
    if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0
        || !targetSize.isValid() || targetSize.width() <= 0 || targetSize.height() <= 0)
    {
        return QImage{};
    }

    QImage thumb = (frame.size() == targetSize)
        ? frame
        : frame.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    if (thumb.isNull())
    {
        return QImage{};
    }

    if (thumb.format() == QImage::Format_Grayscale8)
    {
        return thumb;
    }

    if (thumb.format() != QImage::Format_RGB32 && thumb.format() != QImage::Format_ARGB32)
    {
        thumb = thumb.convertToFormat(QImage::Format_RGB32);
        if (thumb.isNull())
        {
            return QImage{};
        }
    }

    QImage gray(thumb.size(), QImage::Format_Grayscale8);
    const int width = thumb.width();
    const int height = thumb.height();
    for (int y = 0; y < height; ++y)
    {
        const QRgb *srcLine = reinterpret_cast<const QRgb *>(thumb.constScanLine(y));
        uchar *dstLine = gray.scanLine(y);
        for (int x = 0; x < width; ++x)
        {
            dstLine[x] = static_cast<uchar>(qGray(srcLine[x]));
        }
    }
    return gray;
}

void MotionDetector::computeWeights(double motionScore,
                                    const Config &cfg,
                                    double &wQ,
                                    double &wR,
                                    double &wP)
{
    const double m = std::clamp(std::isfinite(motionScore) ? motionScore : 0.0, 0.0, 1.0);
    wQ = std::max(0.0, cfg.wQBase - cfg.wQSlope * m);
    wR = std::max(0.0, cfg.wRBase - cfg.wRSlope * m);
    wP = std::max(0.0, cfg.wPBase + cfg.wPSlope * m);

    const double sum = wQ + wR + wP;
    if (sum > 1e-12 && std::isfinite(sum))
    {
        wQ /= sum;
        wR /= sum;
        wP /= sum;
    }
    else
    {
        wQ = 0.35;
        wR = 0.50;
        wP = 0.15;
    }
}

MotionResult MotionDetector::analyze(const QImage &frame)
{
    QMutexLocker locker(&m_mutex);

    const QImage curr = toGrayThumbnail(frame, m_config.thumbnailSize);
    if (curr.isNull() || curr.size() != m_config.thumbnailSize || curr.format() != QImage::Format_Grayscale8)
    {
        MotionResult invalidResult;
        invalidResult.rawScore = 0.0;
        invalidResult.smoothedScore = m_smoothedScore;
        computeWeights(m_smoothedScore, m_config, invalidResult.wQ, invalidResult.wR, invalidResult.wP);
        invalidResult.valid = false;
        return invalidResult;
    }

    if (!m_hasPrevious || m_prevThumbnail.size() != curr.size() || m_prevThumbnail.format() != QImage::Format_Grayscale8)
    {
        m_prevThumbnail = curr;
        m_hasPrevious = true;
        m_smoothedScore = 0.0;

        MotionResult firstResult;
        firstResult.rawScore = 0.0;
        firstResult.smoothedScore = 0.0;
        computeWeights(0.0, m_config, firstResult.wQ, firstResult.wR, firstResult.wP);
        firstResult.valid = true;
        return firstResult;
    }

    const int width = curr.width();
    const int height = curr.height();
    const int totalPixels = width * height;
    int changedPixels = 0;

    for (int y = 0; y < height; ++y)
    {
        const uchar *currLine = curr.constScanLine(y);
        const uchar *prevLine = m_prevThumbnail.constScanLine(y);
        for (int x = 0; x < width; ++x)
        {
            const int diff = std::abs(static_cast<int>(currLine[x]) - static_cast<int>(prevLine[x]));
            if (diff > m_config.pixelDiffThreshold)
            {
                ++changedPixels;
            }
        }
    }

    const double rawScore = (totalPixels > 0)
        ? std::clamp(static_cast<double>(changedPixels) / static_cast<double>(totalPixels), 0.0, 1.0)
        : 0.0;
    m_smoothedScore = std::clamp(
        m_config.emaAlpha * rawScore + (1.0 - m_config.emaAlpha) * m_smoothedScore,
        0.0,
        1.0);
    m_prevThumbnail = curr;

    MotionResult result;
    result.rawScore = rawScore;
    result.smoothedScore = m_smoothedScore;
    computeWeights(m_smoothedScore, m_config, result.wQ, result.wR, result.wP);
    result.valid = true;
    return result;
}

void MotionDetector::reset()
{
    QMutexLocker locker(&m_mutex);
    m_prevThumbnail = QImage();
    m_smoothedScore = 0.0;
    m_hasPrevious = false;
}

void MotionDetector::setConfig(const Config &config)
{
    QMutexLocker locker(&m_mutex);
    const Config sanitized = sanitizeConfig(config);
    if (sanitized.thumbnailSize != m_config.thumbnailSize)
    {
        m_prevThumbnail = QImage();
        m_hasPrevious = false;
    }
    m_config = sanitized;
}

MotionDetector::Config MotionDetector::config() const
{
    QMutexLocker locker(&m_mutex);
    return m_config;
}

double MotionDetector::smoothedScore() const
{
    QMutexLocker locker(&m_mutex);
    return m_smoothedScore;
}
