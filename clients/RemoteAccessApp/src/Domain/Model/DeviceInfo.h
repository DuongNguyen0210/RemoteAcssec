#pragma once
#include <QString>

// One currently heartbeating login session; no persistent device inventory.
struct DeviceInfo {
    QString sessionId;
    qint64 childId = 0;
    QString username;
    QString deviceName;
    QString os;
    QString ipAddress;
    qint64 lastHeartbeatAt = 0;
};
