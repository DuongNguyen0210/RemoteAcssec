#include "DeviceService.h"
#include "ApiClient.h"
#include <QJsonArray>

void DeviceService::fetchDevices()
{
    if (m_loading) return;
    auto *reply = ApiClient::instance().get("/api/v1/devices");
    if (!reply) {
        emit devicesResult(false, {}, QStringLiteral("Không thể gửi yêu cầu lấy thiết bị"));
        return;
    }
    m_loading = true;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_loading = false;
        auto response = ApiClient::parseReply(reply);
        reply->deleteLater();
        if (!response.success || !response.data.isArray()) {
            emit devicesResult(false, {}, response.message.isEmpty()
                    ? QStringLiteral("Không thể cập nhật danh sách thiết bị") : response.message);
            return;
        }
        QList<DeviceInfo> devices;
        for (const auto &value : response.data.toArray()) {
            auto obj = value.toObject();
            DeviceInfo device;
            device.sessionId = obj.value("sessionId").toString();
            device.childId = obj.value("childId").toVariant().toLongLong();
            device.username = obj.value("username").toString();
            device.deviceName = obj.value("deviceName").toString();
            device.os = obj.value("os").toString();
            device.ipAddress = obj.value("ipAddress").toString();
            device.lastHeartbeatAt = obj.value("lastHeartbeatAt").toVariant().toLongLong();
            if (device.sessionId.isEmpty()) {
                emit devicesResult(false, {}, QStringLiteral("Thiết bị thiếu ID phiên đăng nhập"));
                return;
            }
            devices.append(device);
        }
        emit devicesResult(true, devices, response.message);
    });
}
