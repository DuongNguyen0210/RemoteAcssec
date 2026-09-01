#ifndef SCREENENCODER_H
#define SCREENENCODER_H

#include <QByteArray>
#include <QImage>

// ---------------------------------------------------------------------------
// ScreenEncoder
//
// GRASP responsibility: Information Expert for screen frame encoding.
//
// Accepts a raw QImage and returns a JPEG-compressed QByteArray.
// This class knows NOTHING about:
//   - ScreenCapture internals  (only consumes QImage)
//   - RelayClient / QTcpSocket
//   - RDTP / SCREEN_FRAME / ProtocolHeader
//   - JWT / ApiClient / Management API
//   - session IDs or routing
//   - chunking logic
//
// It is designed to be called by a future ScreenFramePacket builder that
// wraps the encoded bytes into an RDTP payload and ships them.
//
// All methods are static — no object state is needed.
// ---------------------------------------------------------------------------

class ScreenEncoder
{
public:
    // Encode a raw QImage to a JPEG-compressed QByteArray.
    //
    // Parameters:
    //   image   — source image; must not be null.
    //   quality — JPEG quality [1, 100].
    //             Qt semantics: 1 = smallest / worst, 100 = largest / best.
    //             -1 is also accepted by Qt as "use Qt default (~75)".
    //             Values outside [-1, 100] are clamped to [1, 100].
    //
    // Returns:
    //   Non-empty QByteArray containing valid JPEG data on success.
    //   Empty QByteArray on failure (null image, QBuffer/save error).
    //
    // Never throws. Never crashes. Never blocks indefinitely.
    static QByteArray encodeJpeg(const QImage &image, int quality = 75);

    // Default JPEG quality used by encodeJpeg() when no argument is given.
    static constexpr int DEFAULT_QUALITY = 75;
};

#endif // SCREENENCODER_H
