#include "ScreenEncoder.h"

#include <QBuffer>
#include <QDebug>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <utility>

QByteArray ScreenEncoder::encodeJpeg(const QImage &image, int quality)
{
    if (image.isNull()) {
        qWarning() << "[ScreenEncoder] encodeJpeg() called with a null QImage.";
        return QByteArray{};
    }

    if (quality != -1) {
        quality = std::clamp(quality, 1, 100);
    }

    QByteArray buffer;
    QBuffer qbuf(&buffer);

    if (!qbuf.open(QIODevice::WriteOnly)) {
        qWarning() << "[ScreenEncoder] QBuffer::open(WriteOnly) failed.";
        return QByteArray{};
    }

    const bool saved = image.save(&qbuf, "JPEG", quality);

    if (!saved || buffer.isEmpty()) {
        qWarning() << "[ScreenEncoder] QImage::save() to JPEG failed."
                   << "size=" << image.size()
                   << "format=" << image.format()
                   << "quality=" << quality;
        return QByteArray{};
    }

    return buffer;
}

EncodedScreenFrame ScreenEncoder::encodeWithinLimit(const QImage &image, qsizetype maxBytes)
{
    if (image.isNull() || maxBytes <= 0) return {};
    QImage candidate = image;
    while (!candidate.isNull()) {
        QByteArray jpeg = encodeJpeg(candidate);
        if (jpeg.isEmpty()) return {};
        if (jpeg.size() <= maxBytes) return {std::move(jpeg), candidate.size()};
        if (candidate.width() <= 1 || candidate.height() <= 1) break;
        candidate = candidate.scaled(qMax(1, candidate.width() / 2),
                                     qMax(1, candidate.height() / 2),
                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return {};
}

QSize ScreenEncoder::scaledDimensions(const QSize &sourceSize, double scale)
{
    if (!sourceSize.isValid() || sourceSize.width() <= 0 || sourceSize.height() <= 0 ||
        !std::isfinite(scale) || scale <= 0.0) {
        return QSize{};
    }

    const double clampedScale = std::clamp(scale, 0.0, 1.0);
    if (clampedScale >= 1.0) {
        return sourceSize;
    }

    const int targetW = std::max(
        1, static_cast<int>(std::lround(static_cast<double>(sourceSize.width()) * clampedScale)));
    const int targetH = std::max(
        1, static_cast<int>(std::lround(static_cast<double>(sourceSize.height()) * clampedScale)));
    return QSize(targetW, targetH);
}

QImage ScreenEncoder::scaleImage(const QImage &image, double scale, Qt::TransformationMode mode)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return QImage{};
    }

    const QSize targetSize = scaledDimensions(image.size(), scale);
    if (!targetSize.isValid()) {
        return QImage{};
    }

    if (targetSize == image.size()) {
        return image;
    }

    return image.scaled(targetSize, Qt::IgnoreAspectRatio, mode);
}

TimedEncodedFrame ScreenEncoder::encodeAt(const QImage &image, int quality)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return TimedEncodedFrame{};
    }

    const int clampedQuality = std::clamp(quality, MIN_QUALITY, MAX_QUALITY);
    QElapsedTimer timer;
    timer.start();
    QByteArray jpeg = encodeJpeg(image, clampedQuality);
    const qint64 elapsedMs = timer.elapsed();

    if (jpeg.isEmpty()) {
        return TimedEncodedFrame{{}, image.size(), clampedQuality, elapsedMs, false};
    }

    return TimedEncodedFrame{std::move(jpeg), image.size(), clampedQuality, elapsedMs, true};
}

TimedEncodedFrame ScreenEncoder::encodeScaledAt(const QImage &image,
                                                double scale,
                                                int quality,
                                                Qt::TransformationMode mode)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return TimedEncodedFrame{};
    }

    QElapsedTimer totalTimer;
    totalTimer.start();
    const QImage scaled = scaleImage(image, scale, mode);
    if (scaled.isNull()) {
        return TimedEncodedFrame{};
    }

    TimedEncodedFrame result = encodeAt(scaled, quality);
    result.encodeTimeMs = totalTimer.elapsed();
    return result;
}
