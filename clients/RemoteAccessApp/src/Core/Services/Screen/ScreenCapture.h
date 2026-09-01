#ifndef SCREENCAPTURE_H
#define SCREENCAPTURE_H

#include <QImage>

// ---------------------------------------------------------------------------
// ScreenCapture
//
// GRASP responsibility: Information Expert for local display access.
//
// Captures the primary screen and returns raw pixel data as QImage.
// This class knows NOTHING about:
//   - RelayClient / QTcpSocket
//   - RDTP / SCREEN_FRAME protocol
//   - JWT / ApiClient / Management API
//   - session IDs, compression, or chunking
//
// It is designed to be called by a future ScreenEncoder/StreamingController
// that transforms and sends the frame.
//
// Thread safety: must be called from the main (GUI) thread.
// ---------------------------------------------------------------------------

class ScreenCapture
{
public:
    ScreenCapture() = default;

    // Capture the entire primary screen.
    //
    // Returns a valid, non-null QImage on success.
    // Returns a null QImage (image.isNull() == true) if:
    //   - no QGuiApplication / primary screen is available
    //   - QScreen::grabWindow() returns a null pixmap
    //
    // Never throws. Never crashes. Never blocks indefinitely.
    QImage capture() const;
};

#endif // SCREENCAPTURE_H
