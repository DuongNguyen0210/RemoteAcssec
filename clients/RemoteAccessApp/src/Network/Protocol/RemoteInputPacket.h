#pragma once
#include <QByteArray>
#include <QtEndian>

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
