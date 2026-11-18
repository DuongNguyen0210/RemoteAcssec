> **Hiện tại: RDTP v2.** Xem [hợp đồng xác thực theo phiên](11_SESSION_AUTH_V2.md). Các tài liệu 01–10 bên dưới mô tả nền tảng v1; phần version và payload đăng ký/kết nối được thay thế bởi tài liệu v2.

# RDTP Protocol Documentation

> Tai lieu ky thuat cho RemoteAccessApp - RDTP (Remote Desktop Transfer Protocol)
> Phạm vi: nền tảng RDTP, khám phá thiết bị, xác thực, vòng đời phiên và truyền màn hình.

---

## Danh sach tai lieu

| File | Mo ta |
|---|---|
| [01_PROTOCOL_OVERVIEW.md](01_PROTOCOL_OVERVIEW.md) | Tong quan RDTP: protocol la gi, packet, header, payload, wire format, moi quan he Host/Client/Relay |
| [02_PROTOCOL_CONSTANTS.md](02_PROTOCOL_CONSTANTS.md) | Cac hang so: PROTOCOL_MAGIC, PROTOCOL_VERSION, HEADER_SIZE, MAX_PAYLOAD_LENGTH |
| [03_MESSAGE_TYPES.md](03_MESSAGE_TYPES.md) | Hợp đồng wire của đăng ký CHILD, handshake và SCREEN_FRAME theo phiên |
| [04_PROTOCOL_HEADER_MODEL.md](04_PROTOCOL_HEADER_MODEL.md) | struct ProtocolHeader: tung field, default values, constructor, vi du |
| [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md) | 24-byte wire format: offset layout, ly do khong serialize raw struct |
| [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) | ProtocolSerializer: serializeHeader(), QByteArray, Q_ASSERT, test vector |
| [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md) | Big Endian / Network Byte Order: ly thuyet, cac helper appendUIntXXBE |
| [08_CMAKE_INTEGRATION.md](08_CMAKE_INTEGRATION.md) | CMakeLists.txt: NETWORK_SOURCES, target_include_directories |
| [09_IMPLEMENTATION_STATUS.md](09_IMPLEMENTATION_STATUS.md) | Trạng thái hiện tại và lịch sử triển khai |
| [10_HEADER_DESERIALIZATION.md](10_HEADER_DESERIALIZATION.md) | Giai thich Phase 1A.2C: chuyen 24-byte RDTP wire header thanh ProtocolHeader object. |
| [11_SESSION_AUTH_V2.md](11_SESSION_AUTH_V2.md) | Hợp đồng RDTP v2 cho đăng ký CHILD và mở phiên ADMIN |

---

## Sơ đồ lịch sử RDTP v1

```
02_PROTOCOL_CONSTANTS.md  (constants + MessageType)
        |
        v
04_PROTOCOL_HEADER_MODEL.md  (struct ProtocolHeader)
        |
        v
05_WIRE_HEADER_FORMAT.md  (layout 24-byte wire)
        |
        v
06_HEADER_SERIALIZATION.md  (ProtocolSerializer)
        |
        v
RelayClient TCP -> REGISTER_HOST
        |
        v
ProtocolDecoder -> RelayServerHandler -> RelayRegistry
        |
        v
ProtocolEncoder -> REGISTER_ACK
        |
        v
RdtpStreamParser -> ScreenStreamSender
        |
        v
ADMIN chọn childUsername thật -> AdminSessionController -> CONNECT_REQUEST
        |
        v
RelayRegistry cấp sessionId -> SESSION_REQUEST -> CHILD SESSION_ACCEPT
        |
        v
Relay ACTIVE -> CONNECT_RESULT -> ADMIN lưu active sessionId
        |
        v
CHILD SCREEN_FRAME(sessionId) -> Relay kiểm tra ACTIVE + đúng CHILD
        |
        v
Protocol gốc -> ProtocolEncoder -> channel ADMIN
```

Ly thuyet byte-order xem: [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md)

CMake build xem: [08_CMAKE_INTEGRATION.md](08_CMAKE_INTEGRATION.md)

## Luồng khám phá CHILD của ADMIN trong RDTP v1

```text
ADMIN đăng nhập và nhận JWT
        |
        v
GET /api/v1/child
        |
        v
JWT subject -> User -> ChildRepository.findByOwner(...)
        |
        v
childUsername thật -> ChildDiscoveryService -> DevicesPage
        |
        v
DeviceCardWidget -> MainWindow -> AppController
```

Giai đoạn 3B tiếp tục từ `AppController`: `AdminSessionController` kết nối hoặc
tái sử dụng `RelayClient`, gửi `CONNECT_REQUEST`, nhận `CONNECT_RESULT` và lưu
`active sessionId`. Relay quản lý trạng thái `PENDING`/`ACTIVE`, còn CHILD phản
hồi `SESSION_ACCEPT` hoặc `SESSION_REJECT` qua `ScreenStreamSender`.

Giai đoạn 4 dùng chính ID `ACTIVE` đó trong header của từng `SCREEN_FRAME`.
`RelayRegistry` đồng thời kiểm tra trạng thái phiên và channel CHILD nguồn trước
khi `RelayServerHandler` chuyển đối tượng `Protocol` gốc đến ADMIN. Việc ghép
khung chẩn đoán ở Relay chạy sau và không tham gia quyết định định tuyến.

## Luồng hiện tại

CHILD đăng ký với Relay bằng JWT. ADMIN chọn thiết bị từ `GET /api/v1/devices`
và mở phiên theo UUID đăng nhập của thiết bị. Sau khi phiên được chấp nhận,
CHILD gửi các chunk JPEG qua Relay để ADMIN ghép, giải mã và hiển thị.
ADMIN cũng gửi thao tác chuột/bàn phím cho CHILD. CHILD hỗ trợ Windows và X11; Wayland chưa hỗ trợ. Hai phía xử lý kết thúc phiên qua `DISCONNECT`. Xem
[trạng thái triển khai](09_IMPLEMENTATION_STATUS.md) và
[hợp đồng xác thực](11_SESSION_AUTH_V2.md).
