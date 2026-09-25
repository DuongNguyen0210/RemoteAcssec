#pragma once
#include <QString>
#include <QMetaType>
#include <QDateTime>

struct RelayEndpoint {
    QString instanceId;
    QString host;
    quint16 port = 0;
    qint64 expiresAt = 0; // epoch milliseconds, zero for static endpoints
    bool isValid() const {
        return !host.trimmed().isEmpty() && port != 0
                && (expiresAt == 0 || expiresAt > QDateTime::currentMSecsSinceEpoch());
    }
    bool sameAddress(const RelayEndpoint &other) const {
        return instanceId == other.instanceId && host == other.host && port == other.port;
    }
};
Q_DECLARE_METATYPE(RelayEndpoint)
