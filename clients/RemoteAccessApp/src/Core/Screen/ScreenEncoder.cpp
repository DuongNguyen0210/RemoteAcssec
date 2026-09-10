#include "ScreenEncoder.h"

#include <QBuffer>
#include <QDebug>
#include <algorithm>   // std::clamp

// ---------------------------------------------------------------------------
// encodeJpeg()
//
// Implementation notes:
//   1. Null QImage check — if the caller passes a null frame (e.g. from a
//      failed ScreenCapture), we bail immediately with an empty QByteArray.
//
//   2. Quality clamping — Qt accepts [-1, 100].  -1 means "default".
//      Values outside that range are clamped to [1, 100] so callers cannot
//      accidentally pass 0 (which Qt treats as very low but still valid) or
//      values > 100 that are simply ignored.
//
//   3. QBuffer + QImage::save() — the standard Qt way to encode in-memory
//      without touching the filesystem.  QBuffer wraps a QByteArray and
//      presents a QIODevice interface that QImage::save() writes to.
//
//   4. On QBuffer::open() failure or QImage::save() failure we return an
//      empty QByteArray and log a warning.  We do NOT throw.
// ---------------------------------------------------------------------------

QByteArray ScreenEncoder::encodeJpeg(const QImage &image, int quality)
{
    // --- guard: null image ---------------------------------------------------
    if (image.isNull()) {
        qWarning() << "[ScreenEncoder] encodeJpeg() called with a null QImage.";
        return QByteArray{};
    }

    // --- clamp quality -------------------------------------------------------
    // Allow Qt's special -1 ("use default") through; clamp everything else.
    if (quality != -1) {
        quality = std::clamp(quality, 1, 100);
    }

    // --- encode to in-memory buffer ------------------------------------------
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
