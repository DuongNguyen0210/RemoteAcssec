#ifndef SCREENCAPTURE_H
#define SCREENCAPTURE_H

#include <QImage>

class ScreenCapture
{
public:
    ScreenCapture() = default;

    QImage capture() const;
};

#endif
