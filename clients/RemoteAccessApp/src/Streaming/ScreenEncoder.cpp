#include "ScreenEncoder.h"

#include <QBuffer>
#include <QDebug>
#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>
#include <utility>

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

    return buffer;
}

namespace {
constexpr int minimumScale = 10;
constexpr int minimumQuality = 45;
constexpr int step = 5;
constexpr int scaleCount = (100 - minimumScale) / step + 1;
constexpr int qualityCount = (ScreenEncoder::DEFAULT_QUALITY - minimumQuality) / step + 1;

QSize scaledSize(QSize source, int percent)
{
    // Integer rounding also avoids overflowing width * percent on a large image.
    return {qMax(1, int((qint64(source.width()) * percent + 50) / 100)),
            qMax(1, int((qint64(source.height()) * percent + 50) / 100))};
}

struct Probe {
    QSize size;
    int quality = 0;
    qsizetype bytes = 0;
};
}

EncodedScreenFrame ScreenEncoder::encodeWithinLimit(const QImage &image, qsizetype maxBytes,
                                                    ScreenEncodeState state, bool allowUpgrade,
                                                    JpegEncoder encode)
{
    EncodedScreenFrame result;
    if (state.sourceSize != image.size()) {
        state = {};
        state.sourceSize = image.size();
    }
    // These bounds are hints from previous captures. Re-open them when the
    // budget increases or the warm JPEG becomes substantially cheaper.
    const auto reopenSearch = [&] {
        state.upperScale = 100 + step;
        state.upperQuality = DEFAULT_QUALITY + step;
        state.framesUntilAudit = 0;
    };
    if (state.previousBudget && maxBytes > state.previousBudget * 1.2)
        reopenSearch();
    state.previousBudget = maxBytes;
    result.nextState = state;
    if (image.isNull() || maxBytes <= 0 || !encode) return result;

    const QSize smallest = scaledSize(image.size(), minimumScale);
    std::array<Probe, MAX_ATTEMPTS> probes;
    int bestScale = state.scale;
    bool minimumTried = false;
    auto tryProfile = [&](int scale, int quality) -> qsizetype {
        const QSize size = scaledSize(image.size(), scale);
        for (int i = 0; i < result.attempts; ++i) {
            if (probes[i].size != size || probes[i].quality != quality) continue;
            // Tiny captures may map adjacent grid points to identical pixels.
            // Move the bracket even when reusing this measurement.
            const qsizetype bytes = probes[i].bytes;
            if (bytes > 0 && bytes <= maxBytes && result.size == size) {
                bestScale = qMax(bestScale, scale);
            } else if (bytes == 0 || bytes > maxBytes) {
                if (quality == minimumQuality) state.upperScale = qMin(state.upperScale, scale);
                if (scale == bestScale) state.upperQuality = qMin(state.upperQuality, quality);
            }
            return bytes;
        }
        if (result.attempts == MAX_ATTEMPTS) return 0;
        const QImage candidate = size == image.size() ? image
                : image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QByteArray jpeg = encode(candidate, quality);
        const qsizetype bytes = jpeg.size();
        probes[result.attempts++] = {size, quality, bytes};
        minimumTried |= (size == smallest && quality == minimumQuality);
        if (bytes == 0 || bytes > maxBytes) {
            if (quality == minimumQuality) state.upperScale = qMin(state.upperScale, scale);
            if (scale == bestScale) state.upperQuality = qMin(state.upperQuality, quality);
            return bytes;
        }

        // Both dimensions grow monotonically with scale (including rounding).
        const auto rank = std::make_tuple(qint64(size.width()) * size.height(), quality);
        const auto best = std::make_tuple(qint64(result.size.width()) * result.size.height(), result.quality);
        if (result.jpeg.isEmpty() || rank > best) {
            if (bestScale != scale) state.upperQuality = DEFAULT_QUALITY + step;
            bestScale = scale;
            result.size = size;
            result.quality = quality;
            result.jpeg = std::move(jpeg);
            if (state.upperScale <= scale) state.upperScale = 100 + step;
        }
        return bytes;
    };

    qsizetype bytes = tryProfile(state.scale, state.quality);
    if (!result.jpeg.isEmpty() && state.previousBytes && bytes < state.previousBytes * 0.8)
        reopenSearch();

    if (result.jpeg.isEmpty() && state.quality > minimumQuality)
        bytes = tryProfile(state.scale, minimumQuality);

    if (result.jpeg.isEmpty() && state.scale > minimumScale) {
        // Pixel area predicts a useful first fallback. Only the encoded byte
        // count decides whether that prediction is admitted.
        int fallbackScale = minimumScale;
        if (bytes > maxBytes) {
            const double ratio = std::sqrt(double(maxBytes) / bytes);
            fallbackScale = std::clamp(int(state.scale * ratio / step) * step,
                                       minimumScale, state.scale - step);
        }
        tryProfile(fallbackScale, minimumQuality);
        if (result.jpeg.isEmpty())
            tryProfile(minimumScale, minimumQuality);
    }

    // Spend the remaining attempts on scale first, then on JPEG quality.
    // Periodically audit a higher-ranked profile outside the current bracket
    // so a single nonmonotonic sample cannot permanently exclude it.
    bool auditDue = state.framesUntilAudit == 0;
    if (allowUpgrade && state.framesUntilAudit > 0) --state.framesUntilAudit;
    for (int i = 1; i < MAX_ATTEMPTS && result.attempts < MAX_ATTEMPTS; ++i) {
        // Recovery must also progress before the first successful send and ACK.
        const bool recovering = result.jpeg.isEmpty() && minimumTried;
        if (!allowUpgrade && !recovering) break;
        if (!result.jpeg.isEmpty() && state.upperScale - bestScale > step) {
            const int middle = bestScale + ((state.upperScale - bestScale) / (2 * step)) * step;
            tryProfile(middle, minimumQuality);
        } else if (!result.jpeg.isEmpty() && state.upperQuality - result.quality > step) {
            const int middle = result.quality + ((state.upperQuality - result.quality) / (2 * step)) * step;
            tryProfile(bestScale, middle);
        } else {
            const int bestIndex = result.jpeg.isEmpty() ? -1
                    : (bestScale - minimumScale) / step * qualityCount + (result.quality - minimumQuality) / step;
            const int higherProfiles = scaleCount * qualityCount - bestIndex - 1;
            if (higherProfiles == 0 || (!recovering && !auditDue)) break;
            const int rank = bestIndex + 1 + (state.auditIndex % higherProfiles);
            state.auditIndex = (state.auditIndex + 1) % higherProfiles;
            tryProfile(minimumScale + (rank / qualityCount) * step,
                       minimumQuality + (rank % qualityCount) * step);
            state.framesUntilAudit = 4;
            auditDue = false;
        }
    }

    state.previousBytes = result.jpeg.size();
    state.scale = result.jpeg.isEmpty() ? minimumScale : bestScale;
    state.quality = result.jpeg.isEmpty() ? minimumQuality : result.quality;
    result.nextState = state;
    return result;
}
