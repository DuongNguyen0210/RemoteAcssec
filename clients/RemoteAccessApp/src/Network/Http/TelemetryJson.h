#pragma once
#include "Domain/Model/TelemetryInfo.h"
#include <QJsonObject>
#include <QJsonValue>
#include <cmath>

namespace TelemetryJson {
inline bool number(const QJsonValue &v, double minimum, double maximum) {
    return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() >= minimum && v.toDouble() <= maximum;
}
inline QJsonObject encode(const TelemetryInfo &s, qint64 sequence) {
    QJsonObject o{{"sequence", double(sequence)}};
    o["cpuPercent"] = s.cpuPercent ? QJsonValue(*s.cpuPercent) : QJsonValue(QJsonValue::Null);
    o["memoryUsedBytes"] = s.memoryUsedBytes ? QJsonValue(double(*s.memoryUsedBytes)) : QJsonValue(QJsonValue::Null);
    o["memoryTotalBytes"] = s.memoryTotalBytes ? QJsonValue(double(*s.memoryTotalBytes)) : QJsonValue(QJsonValue::Null);
    o["networkRxBytesPerSecond"] = s.networkRxBytesPerSecond ? QJsonValue(*s.networkRxBytesPerSecond) : QJsonValue(QJsonValue::Null);
    o["networkTxBytesPerSecond"] = s.networkTxBytesPerSecond ? QJsonValue(*s.networkTxBytesPerSecond) : QJsonValue(QJsonValue::Null);
    return o;
}
inline std::optional<TelemetryInfo> decode(const QJsonValue &value, const QJsonValue &age) {
    if (!value.isObject() || !number(age, 0, 9007199254740991.0)) return {};
    const auto o = value.toObject();
    if (!number(o["sequence"], 1, 9007199254740991.0) || std::floor(o["sequence"].toDouble()) != o["sequence"].toDouble()) return {};
    TelemetryInfo s; s.sequence = qint64(o["sequence"].toDouble()); s.ageMs = qint64(age.toDouble());
    if (!o["cpuPercent"].isNull() && !o["cpuPercent"].isUndefined()) {
        if (!number(o["cpuPercent"], 0, 100)) return {};
        s.cpuPercent = o["cpuPercent"].toDouble();
    }
    const bool hasMemory = !o["memoryTotalBytes"].isNull() && !o["memoryTotalBytes"].isUndefined();
    const bool hasUsed = !o["memoryUsedBytes"].isNull() && !o["memoryUsedBytes"].isUndefined();
    if (hasMemory != hasUsed) return {};
    if (hasMemory) {
        if (!number(o["memoryTotalBytes"], 1, 9007199254740991.0)
                || !number(o["memoryUsedBytes"], 0, o["memoryTotalBytes"].toDouble())
                || std::floor(o["memoryTotalBytes"].toDouble()) != o["memoryTotalBytes"].toDouble()
                || std::floor(o["memoryUsedBytes"].toDouble()) != o["memoryUsedBytes"].toDouble()) return {};
        s.memoryTotalBytes = quint64(o["memoryTotalBytes"].toDouble());
        s.memoryUsedBytes = quint64(o["memoryUsedBytes"].toDouble());
    }
    const bool hasRx = !o["networkRxBytesPerSecond"].isNull() && !o["networkRxBytesPerSecond"].isUndefined();
    const bool hasTx = !o["networkTxBytesPerSecond"].isNull() && !o["networkTxBytesPerSecond"].isUndefined();
    if (hasRx != hasTx) return {};
    if (hasRx) {
        if (!number(o["networkRxBytesPerSecond"], 0, 1e15) || !number(o["networkTxBytesPerSecond"], 0, 1e15)) return {};
        s.networkRxBytesPerSecond = o["networkRxBytesPerSecond"].toDouble();
        s.networkTxBytesPerSecond = o["networkTxBytesPerSecond"].toDouble();
    }
    return s;
}
}
