# RDTP Protocol Documentation

> Tai lieu ky thuat cho RemoteAccessApp - RDTP (Remote Desktop Transfer Protocol)
> Phạm vi: nền tảng RDTP, đăng ký CHILD, khám phá CHILD của ADMIN và handshake phiên MVP Giai đoạn 3B

---

## Danh sach tai lieu

| File | Mo ta |
|---|---|
| [01_PROTOCOL_OVERVIEW.md](01_PROTOCOL_OVERVIEW.md) | Tong quan RDTP: protocol la gi, packet, header, payload, wire format, moi quan he Host/Client/Relay |
| [02_PROTOCOL_CONSTANTS.md](02_PROTOCOL_CONSTANTS.md) | Cac hang so: PROTOCOL_MAGIC, PROTOCOL_VERSION, HEADER_SIZE, MAX_PAYLOAD_LENGTH |
| [03_MESSAGE_TYPES.md](03_MESSAGE_TYPES.md) | Các loại thông điệp và hợp đồng wire của đăng ký CHILD cùng handshake phiên MVP |
| [04_PROTOCOL_HEADER_MODEL.md](04_PROTOCOL_HEADER_MODEL.md) | struct ProtocolHeader: tung field, default values, constructor, vi du |
| [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md) | 24-byte wire format: offset layout, ly do khong serialize raw struct |
| [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) | ProtocolSerializer: serializeHeader(), QByteArray, Q_ASSERT, test vector |
| [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md) | Big Endian / Network Byte Order: ly thuyet, cac helper appendUIntXXBE |
| [08_CMAKE_INTEGRATION.md](08_CMAKE_INTEGRATION.md) | CMakeLists.txt: NETWORK_SOURCES, target_include_directories |
| [09_IMPLEMENTATION_STATUS.md](09_IMPLEMENTATION_STATUS.md) | Trạng thái triển khai qua Giai đoạn 3B, bằng chứng runtime và phần chưa triển khai |
| [10_HEADER_DESERIALIZATION.md](10_HEADER_DESERIALIZATION.md) | Giai thich Phase 1A.2C: chuyen 24-byte RDTP wire header thanh ProtocolHeader object. |

---

## So do phu thuoc

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
```

Ly thuyet byte-order xem: [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md)

CMake build xem: [08_CMAKE_INTEGRATION.md](08_CMAKE_INTEGRATION.md)

## Luồng khám phá CHILD của ADMIN

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

Handshake phiên đã được kiểm tra runtime với cùng một `sessionId` khác `0` ở
Relay, CHILD và ADMIN. Việc gắn session vào `SCREEN_FRAME`, chuyển tiếp và hiển
thị màn hình vẫn chưa được triển khai. Chi tiết xem
[09_IMPLEMENTATION_STATUS.md](09_IMPLEMENTATION_STATUS.md).

---

*Cập nhật lần cuối: Giai đoạn 3B handshake phiên MVP đã hoàn thành và kiểm tra runtime thành công.*
