# 09 - Trạng thái triển khai

> Cập nhật lần cuối: Giai đoạn 2 đăng ký CHILD đã hoàn thành và kiểm tra runtime thành công
> Xem them: [README.md](README.md)

---

## Da hoan thanh

### Phase 1A.1 - Protocol Constants & Message Types

- [x] Hang so `PROTOCOL_MAGIC` (0x52445450 = "RDTP")
- [x] Hang so `PROTOCOL_VERSION` (1)
- [x] Hang so `HEADER_SIZE` (24)
- [x] Hang so `MAX_PAYLOAD_LENGTH` (524288 = 512 KiB)
- [x] `enum class MessageType : uint8_t` voi 20 message types
- [x] File `ProtocolConstants.h` trong `namespace Protocol`
- [x] CMake: `Protocol/ProtocolConstants.h` thuoc target `Network`

### Phase 1A.2A - ProtocolHeader Data Model

- [x] `struct ProtocolHeader` voi 7 fields
- [x] Constructor `explicit ProtocolHeader(MessageType)`
- [x] Default values cho 6/7 fields (tru `type` do caller cung cap)
- [x] File `ProtocolHeader.h` trong `namespace Protocol`
- [x] CMake: `Protocol/ProtocolHeader.h` thuoc target `Network`

### Phase 1A.2B - Header Serialization

- [x] Class `ProtocolSerializer` trong `namespace Protocol`
- [x] `static QByteArray serializeHeader(const ProtocolHeader &header)`
- [x] Serialize theo dung RDTP wire format (24 bytes)
- [x] Big Endian / Network Byte Order cho moi integer field
- [x] Helper: `appendUInt8`, `appendUInt16BE`, `appendUInt32BE`, `appendUInt64BE`
- [x] `MessageType` serialize bang underlying `uint8_t` value
- [x] `Q_ASSERT` kiem tra size == 24 bytes
- [x] CMake: `ProtocolSerializer.h` + `ProtocolSerializer.cpp` thuoc target `Network`
- [x] Build PASS
- [x] App launch PASS (kiem tra regression)
- [x] Test vector PASS (xac nhan bang Python struct.pack)

### Phase 1A.2C - Header Deserialization

- [x] `static std::optional<ProtocolHeader> deserializeHeader(const QByteArray &data)`
- [x] Validate size `data.size() == HEADER_SIZE`
- [x] Decode MessageType bang uint8_t raw value (offset 5)
- [x] Helper: `readUInt8`, `readUInt16BE`, `readUInt32BE`, `readUInt64BE`
- [x] Xu ly ép kieu `uint8_t` de tranh sign extension tu `char`
- [x] Khoi tao `ProtocolHeader` tu constructor voi `MessageType`
- [x] Round-trip verification PASS
- [x] Khong su dung raw memory (memcpy, pragma pack, sizeof)

### Giai đoạn 2 - CHILD đăng ký với Relay

Mục đích hiện tại là đăng ký tên đăng nhập CHILD đã được xác thực với đúng kênh
TCP đang kết nối đến Relay. Luồng đã triển khai:

```text
CHILD đăng nhập
→ childUsername đã xác thực được truyền đến ScreenStreamSender
→ RelayClient thiết lập kết nối TCP
→ ScreenStreamSender gửi REGISTER_HOST
→ RelayServerHandler kiểm tra và đăng ký qua RelayRegistry
→ RelayServerHandler gửi REGISTER_ACK qua ProtocolEncoder
→ ScreenStreamSender đọc ACK qua RdtpStreamParser và cập nhật kết quả đăng ký
```

#### Trách nhiệm hiện tại của RelayRegistry

- Lưu ánh xạ `childUsername → Channel`.
- Lưu ánh xạ ngược `ChannelId → childUsername`.
- Chấp nhận lặp lại cùng username trên cùng channel theo cách idempotent.
- Từ chối cùng username trên một channel khác và không thay thế đăng ký đang có.
- Từ chối cùng channel đăng ký một username khác.
- Khi `channelInactive`, chỉ xóa ánh xạ vẫn thuộc về chính channel đã đóng.

#### Các tệp và lớp đang tham gia

| Phía | Lớp | Trách nhiệm hiện tại |
|---|---|---|
| C++ | `AuthService` | Gửi yêu cầu đăng nhập, giữ đúng username tương ứng với phản hồi xác thực và phát kết quả đăng nhập. |
| C++ | `AuthController` | Nhận kết quả xác thực và chuyển tiếp role cùng username đã xác thực. |
| C++ | `AppController` | Giữ luồng ADMIN hiện có; với CHILD, truyền username vào `ScreenStreamSender`. |
| C++ | `RelayClient` | Quản lý kết nối TCP bền, gửi byte thô và phát thông báo kết nối, ngắt kết nối, dữ liệu nhận được. Không chứa ngữ nghĩa đăng ký. |
| C++ | `ScreenStreamSender` | Điều phối đăng ký CHILD, tạo `REGISTER_HOST`, gửi một lần cho mỗi kết nối, nhận `REGISTER_ACK` và giữ trạng thái đăng ký tối thiểu. |
| C++ | `RdtpStreamParser` | Tách thông điệp RDTP từ luồng TCP, gồm ACK bị phân mảnh hoặc nhiều thông điệp nằm trong một lần đọc. |
| Java | `RelayServer` | Tạo một `RelayRegistry` dùng chung và lắp `ProtocolDecoder`, `ProtocolEncoder`, `RelayServerHandler` vào pipeline Netty. |
| Java | `RelayServerHandler` | Phân phối `REGISTER_HOST`, kiểm tra header/payload/UTF-8, gọi registry, gửi `REGISTER_ACK` và dọn đăng ký khi channel đóng. |
| Java | `RelayRegistry` | Sở hữu hai ánh xạ đăng ký CHILD, chính sách trùng lặp và dọn dẹp an toàn theo channel. |
| Java | `ProtocolDecoder` | Giải mã header và payload RDTP từ luồng TCP đầu vào, có kiểm tra giới hạn khung. |
| Java | `ProtocolEncoder` | Mã hóa `Protocol` thành header RDTP 24 byte và payload đầu ra theo Big Endian. |

#### Kết quả xác minh

- [x] C++ build thành công bằng CMake.
- [x] Java runtime thành công.
- [x] Runtime `REGISTER_HOST` thành công.
- [x] Runtime `REGISTER_ACK` thành công.
- [x] Bionic review thành công.
- [x] Hợp đồng Big Endian đã được xác minh.


---

## Chưa triển khai

Các chức năng dưới đây **chưa được triển khai** trong phạm vi hiện tại:

- Kết nối ADMIN đến Relay.
- `CONNECT_REQUEST`.
- Cấp phát `sessionId` và quản lý phiên.
- `SESSION_REQUEST`, `SESSION_ACCEPT`, `SESSION_REJECT`.
- Chuyển tiếp `SCREEN_FRAME` từ CHILD đến ADMIN.
- Nhận và hiển thị màn hình tại ADMIN.
- Xác thực JWT trên kết nối Relay.

Việc các loại thông điệp tương ứng đã có giá trị trong `MessageType` không có
nghĩa là hành vi phiên hoặc ADMIN đã được triển khai.

---

## Sơ đồ tiến độ

```
[DONE] ProtocolConstants.h  (constants + MessageType)
[DONE] ProtocolHeader.h     (data model)
[DONE] ProtocolSerializer   (serializeHeader -> 24 bytes)
[DONE] Header deserializer   (deserializeHeader <- 24 bytes)
[DONE] RdtpStreamParser      (phân khung luồng TCP)
[DONE] Relay TCP transport
[DONE] CHILD REGISTER_HOST / REGISTER_ACK
[DONE] RelayRegistry cho đăng ký CHILD
[ ]    Kết nối ADMIN đến Relay
[ ]    Cấp phát và quản lý phiên
[ ]    Chuyển tiếp SCREEN_FRAME đến ADMIN
[ ]    Hiển thị màn hình tại ADMIN
[ ]    Xác thực JWT trên Relay
```

---

## Tai lieu lien quan

- [01_PROTOCOL_OVERVIEW.md](01_PROTOCOL_OVERVIEW.md) - Tong quan va luong su dung
- [02_PROTOCOL_CONSTANTS.md](02_PROTOCOL_CONSTANTS.md) - Chi tiet Phase 1A.1 constants
- [04_PROTOCOL_HEADER_MODEL.md](04_PROTOCOL_HEADER_MODEL.md) - Chi tiet Phase 1A.2A
- [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) - Chi tiet Phase 1A.2B
- [10_HEADER_DESERIALIZATION.md](10_HEADER_DESERIALIZATION.md) - Chi tiet Phase 1A.2C
- [03_MESSAGE_TYPES.md](03_MESSAGE_TYPES.md) - Hợp đồng wire của REGISTER_HOST và REGISTER_ACK
