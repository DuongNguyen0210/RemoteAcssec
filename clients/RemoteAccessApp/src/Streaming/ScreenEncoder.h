#ifndef SCREENENCODER_H
#define SCREENENCODER_H

#include <QByteArray>
#include <QImage>

class ScreenEncoder
{
public:
    static QByteArray encodeJpeg(const QImage &image, int quality = 75);

    static constexpr int DEFAULT_QUALITY = 75;
};

#endif
