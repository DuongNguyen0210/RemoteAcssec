#ifndef SCREENENCODER_H
#define SCREENENCODER_H

#include <QByteArray>
#include <QImage>
#include <QSize>

struct EncodedScreenFrame {
    QByteArray jpeg;
    QSize size;
};

class ScreenEncoder
{
public:
    static QByteArray encodeJpeg(const QImage &image, int quality = 75);
    static EncodedScreenFrame encodeWithinLimit(const QImage &image, qsizetype maxBytes);

    static constexpr int DEFAULT_QUALITY = 75;
};

#endif
