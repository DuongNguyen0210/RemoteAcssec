#include "ScreenEncoder.h"

#include <QBuffer>
#include <QDebug>
#include <algorithm>
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
