# RDTP Protocol Documentation

> Tai lieu ky thuat cho RemoteAccessApp - RDTP (Remote Desktop Transfer Protocol)
> Pham vi: Phase 1A.1, Phase 1A.2A, Phase 1A.2B, Phase 1A.2C

---

## Danh sach tai lieu

| File | Mo ta |
|---|---|
| [01_PROTOCOL_OVERVIEW.md](01_PROTOCOL_OVERVIEW.md) | Tong quan RDTP: protocol la gi, packet, header, payload, wire format, moi quan he Host/Client/Relay |
| [02_PROTOCOL_CONSTANTS.md](02_PROTOCOL_CONSTANTS.md) | Cac hang so: PROTOCOL_MAGIC, PROTOCOL_VERSION, HEADER_SIZE, MAX_PAYLOAD_LENGTH |
| [03_MESSAGE_TYPES.md](03_MESSAGE_TYPES.md) | enum class MessageType: dinh nghia, toan bo bang gia tri, nhom chuc nang |
| [04_PROTOCOL_HEADER_MODEL.md](04_PROTOCOL_HEADER_MODEL.md) | struct ProtocolHeader: tung field, default values, constructor, vi du |
| [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md) | 24-byte wire format: offset layout, ly do khong serialize raw struct |
| [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) | ProtocolSerializer: serializeHeader(), QByteArray, Q_ASSERT, test vector |
| [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md) | Big Endian / Network Byte Order: ly thuyet, cac helper appendUIntXXBE |
| [08_CMAKE_INTEGRATION.md](08_CMAKE_INTEGRATION.md) | CMakeLists.txt: NETWORK_SOURCES, target_include_directories |
| [09_IMPLEMENTATION_STATUS.md](09_IMPLEMENTATION_STATUS.md) | Trang thai implementation: da hoan thanh va chua lam |
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
[TCP send - chua implement]

Va chieu nguoc lai:
[TCP receive - chua implement]
        |
        v
10_HEADER_DESERIALIZATION.md  (ProtocolSerializer)
        |
        v
ProtocolHeader object
```

Ly thuyet byte-order xem: [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md)

CMake build xem: [08_CMAKE_INTEGRATION.md](08_CMAKE_INTEGRATION.md)

---

*Cap nhat lan cuoi: Phase 1A.2C hoan thanh.*