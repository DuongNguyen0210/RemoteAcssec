#include "DeviceService.h"
#include "ApiClient.h"
#include <QJsonArray>
#include <QUuid>
#include <QTimer>

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

void DeviceService::allocateRelay(RelayEndpointProvider::Callback callback)
{
    receiveEndpoint(ApiClient::instance().post("/api/v1/relay/allocations", {}), std::move(callback));
}

void DeviceService::fetchRelayEndpoint(const QString &id, RelayEndpointProvider::Callback callback)
{
    if (QUuid(id).isNull()) {
        callback({}, QStringLiteral("ID phiên máy không hợp lệ."));
        return;
    }
    receiveEndpoint(ApiClient::instance().get("/api/v1/devices/" + QUuid(id).toString(QUuid::WithoutBraces)
                    + "/relay"), std::move(callback));
}

void DeviceService::receiveEndpoint(QNetworkReply *reply, RelayEndpointProvider::Callback callback)
{
    if (!reply) { callback({}, QStringLiteral("Không thể gửi yêu cầu Relay.")); return; }
    auto *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout->start(8000);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback)]() {
        const auto response = ApiClient::parseReply(reply);
        const auto obj = response.data.toObject();
        const double portValue = obj.value("port").toDouble(-1);
        const int port = obj.value("port").toInt(-1);
        RelayEndpoint endpoint{obj.value("instanceId").toString(), obj.value("host").toString(),
            static_cast<quint16>(port > 0 && port <= 65535 ? port : 0),
            obj.value("expiresAt").toVariant().toLongLong()};
        if (!response.success || endpoint.instanceId.isEmpty() || !endpoint.isValid()
                || endpoint.expiresAt <= 0 || portValue != port) {
            callback({}, response.success ? QStringLiteral("Endpoint Relay không hợp lệ hoặc đã hết hạn.")
                    : (response.message.isEmpty() ? QStringLiteral("Không có Relay khả dụng.") : response.message));
            return;
        }
        callback(endpoint, {});
    });
}
