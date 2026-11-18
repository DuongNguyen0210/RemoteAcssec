#pragma once
#include <QByteArray>
#include <QtEndian>
#include <QSize>

namespace RemoteInputPacket {
inline QByteArray position(quint32 x, quint32 y) {
    QByteArray bytes(8, '\0');
    qToBigEndian(x, reinterpret_cast<uchar *>(bytes.data()));
    qToBigEndian(y, reinterpret_cast<uchar *>(bytes.data() + 4));
    return bytes;
}
inline QByteArray button(quint32 x, quint32 y, quint8 id) {
    auto bytes = position(x, y); bytes.append(char(id)); return bytes;
}
inline QByteArray wheel(quint32 x, quint32 y, qint32 delta) {
    auto bytes = position(x, y);
    QByteArray amount(4, '\0');
    qToBigEndian(static_cast<quint32>(delta), reinterpret_cast<uchar *>(amount.data()));
    return bytes + amount;
}
// Optional trailing reference geometry keeps delayed input tied to the viewed frame.
inline QByteArray withFrame(QByteArray bytes, const QSize &frame) {
    return bytes + position(quint32(frame.width()), quint32(frame.height()));
}
inline QSize referenceFrame(const QByteArray &bytes, qsizetype baseLength, const QSize &fallback) {
    if (bytes.size() == baseLength) return fallback;
    if (bytes.size() != baseLength + 8) return {};
    const auto *p = reinterpret_cast<const uchar *>(bytes.constData() + baseLength);
    const auto width = qFromBigEndian<quint32>(p), height = qFromBigEndian<quint32>(p + 4);
    if (!width || !height || width > 32768 || height > 32768) return {};
    return QSize(int(width), int(height));
}
inline QByteArray key(quint32 qtKey) {
    QByteArray bytes(4, '\0');
    qToBigEndian(qtKey, reinterpret_cast<uchar *>(bytes.data()));
    return bytes;
}
inline bool readPosition(const QByteArray &bytes, quint32 &x, quint32 &y) {
    if (bytes.size() < 8) return false;
    x = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData()));
    y = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + 4));
    return true;
}
}
