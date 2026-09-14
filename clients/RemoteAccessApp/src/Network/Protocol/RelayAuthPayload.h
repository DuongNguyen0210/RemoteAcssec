#pragma once
#include <QByteArray>
#include <QString>

namespace Protocol {
inline QByteArray relayAuthPayload(const QString& token, const QString& targetAgentSessionId = {}) {
    const auto tokenBytes = token.toUtf8();
    const auto targetBytes = targetAgentSessionId.toUtf8();
    if (tokenBytes.isEmpty() || tokenBytes.size() > 4096 || targetBytes.size() > 36) return {};
    QByteArray result;
    auto append = [&result](const QByteArray& value) {
        result.append(static_cast<char>((value.size() >> 8) & 0xff));
        result.append(static_cast<char>(value.size() & 0xff));
        result.append(value);
    };
    append(tokenBytes);
    if (!targetBytes.isEmpty()) append(targetBytes);
    return result;
}
}
