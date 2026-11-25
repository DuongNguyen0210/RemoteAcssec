#ifndef SCREENENCODER_H
#define SCREENENCODER_H

#include <QByteArray>
#include <QImage>
#include <QSize>

class ScreenEncodeState
{
private:
    friend class ScreenEncoder;
    QSize sourceSize;
    int scale = 100;
    int quality = 60;
    int upperScale = 105; // exclusive, in percentage points
    int upperQuality = 65;
    int auditIndex = 0;
    int framesUntilAudit = 0;
    qsizetype previousBudget = 0;
    qsizetype previousBytes = 0;
};

struct EncodedScreenFrame {
    QByteArray jpeg;
    QSize size;
    int quality = 0;
    int attempts = 0;
    ScreenEncodeState nextState;
};

class ScreenEncoder
{
public:
    static constexpr int DEFAULT_QUALITY = 60;
    static constexpr int MAX_ATTEMPTS = 3;

    using JpegEncoder = QByteArray (*)(const QImage &, int);
    static QByteArray encodeJpeg(const QImage &image, int quality = DEFAULT_QUALITY);
    static EncodedScreenFrame encodeWithinLimit(const QImage &image, qsizetype maxBytes,
                                               ScreenEncodeState state = {}, bool allowUpgrade = false,
                                               JpegEncoder encode = encodeJpeg);
};

#endif
