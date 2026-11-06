#ifndef SCREENCAPTURE_H
#define SCREENCAPTURE_H

#include <QImage>

/**
 * @brief Chịu trách nhiệm chụp ảnh màn hình máy tính (Windows Desktop).
 * Sử dụng QScreen::grabWindow tương thích trực tiếp với Win32 GDI.
 */
class ScreenCapture
{
public:
    ScreenCapture() = default;
    ~ScreenCapture() = default;

    /**
     * @brief Chụp toàn bộ màn hình chính.
     * @return QImage chứa khung hình chụp, hoặc QImage rỗng nếu thất bại.
     */
    QImage capture() const;
};

#endif // SCREENCAPTURE_H
