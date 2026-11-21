#ifndef SCREENENCODER_H
#define SCREENENCODER_H

#include <QByteArray>
#include <QImage>
#include <QSize>

struct EncodedScreenFrame {
    QByteArray jpeg;
    QSize size;
};

struct TimedEncodedFrame {
    QByteArray jpeg;
    QSize size;
    int qualityUsed = 0;
    qint64 encodeTimeMs = 0;
    bool valid = false;
};

class ScreenEncoder
{
public:
    static QByteArray encodeJpeg(const QImage &image, int quality = 60);
    static EncodedScreenFrame encodeWithinLimit(const QImage &image, qsizetype maxBytes);

    static QSize scaledDimensions(const QSize &sourceSize, double scale);
    static QImage scaleImage(const QImage &image,
                             double scale,
                             Qt::TransformationMode mode = Qt::SmoothTransformation);
    static TimedEncodedFrame encodeAt(const QImage &image, int quality);
    static TimedEncodedFrame encodeScaledAt(const QImage &image,
                                            double scale,
                                            int quality,
                                            Qt::TransformationMode mode = Qt::SmoothTransformation);

    static constexpr int DEFAULT_QUALITY = 60;
    static constexpr int MIN_QUALITY = 1;
    static constexpr int MAX_QUALITY = 100;
};

#endif
