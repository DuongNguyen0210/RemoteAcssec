# RDTP Protocol Documentation

> Tai lieu ky thuat cho RemoteAccessApp - RDTP (Remote Desktop Transfer Protocol)
> Phạm vi: nền tảng RDTP và Giai đoạn 2 đăng ký CHILD với Relay

---

## Danh sach tai lieu

| File | Mo ta |
|---|---|
| [01_PROTOCOL_OVERVIEW.md](01_PROTOCOL_OVERVIEW.md) | Tong quan RDTP: protocol la gi, packet, header, payload, wire format, moi quan he Host/Client/Relay |
| [02_PROTOCOL_CONSTANTS.md](02_PROTOCOL_CONSTANTS.md) | Cac hang so: PROTOCOL_MAGIC, PROTOCOL_VERSION, HEADER_SIZE, MAX_PAYLOAD_LENGTH |
| [03_MESSAGE_TYPES.md](03_MESSAGE_TYPES.md) | Các loại thông điệp và hợp đồng wire hiện tại của REGISTER_HOST / REGISTER_ACK |
| [04_PROTOCOL_HEADER_MODEL.md](04_PROTOCOL_HEADER_MODEL.md) | struct ProtocolHeader: tung field, default values, constructor, vi du |
| [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md) | 24-byte wire format: offset layout, ly do khong serialize raw struct |
| [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) | ProtocolSerializer: serializeHeader(), QByteArray, Q_ASSERT, test vector |
| [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md) | Big Endian / Network Byte Order: ly thuyet, cac helper appendUIntXXBE |
| [08_CMAKE_INTEGRATION.md](08_CMAKE_INTEGRATION.md) | CMakeLists.txt: NETWORK_SOURCES, target_include_directories |
| [09_IMPLEMENTATION_STATUS.md](09_IMPLEMENTATION_STATUS.md) | Trạng thái triển khai, luồng đăng ký CHILD, trách nhiệm các lớp và phần chưa triển khai |
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
```

Ly thuyet byte-order xem: [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md)

CMake build xem: [08_CMAKE_INTEGRATION.md](08_CMAKE_INTEGRATION.md)

---

*Cập nhật lần cuối: Giai đoạn 2 đăng ký CHILD đã hoàn thành.*
