#include "ScreenEncoder.h"

#include <QBuffer>
#include <QDebug>
#include <algorithm>

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

    qDebug() << "[ScreenEncoder] JPEG encode OK:"
             << "raw-estimate=" << (image.width() * image.height() * 4) << "bytes"
             << "encoded-bytes=" << buffer.size()
             << "quality=" << quality;

    return buffer;
}
