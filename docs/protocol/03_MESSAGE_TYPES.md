# 03 - MessageType

> Dinh nghia trong: `clients/RemoteAccessApp/src/Network/Protocol/ProtocolConstants.h`
> Namespace: `Protocol`
> Xem them: [README.md](README.md)

---

## Dinh nghia

```cpp
enum class MessageType : uint8_t { ... };
```

**`enum class`** (scoped enum) duoc dung thay vi `enum` thuong vi:
- Cac gia tri duoc dong goi trong scope `MessageType::` - tranh xung dot ten
- Khong tu dong chuyen doi sang `int` - an toan hon khi so sanh

**`uint8_t` lam underlying type** vi:
- Header chi danh 1 byte cho truong `type`
- Cho phep toi da 256 loai message - du dung

**Nam trong `namespace Protocol`** de tranh xung dot voi cac enum hoac hang so khac trong project.

---

## Nhóm: Đăng ký CHILD với Relay

| Tên | Hex | Ý nghĩa hiện tại |
|---|---|---|
| `REGISTER_HOST` | `0x01` | CHILD gửi tên đăng nhập đã được xác thực để đăng ký kênh TCP đang kết nối với Relay. |
| `REGISTER_ACK` | `0x02` | Relay trả kết quả chấp nhận hoặc từ chối đăng ký. |

Mục đích của đăng ký là cho phép Relay biết kênh TCP nào đang thuộc về một
`childUsername`. Phạm vi hiện tại chỉ thiết lập ánh xạ CHILD với kênh; chưa tạo
phiên điều khiển và chưa kết nối ADMIN với Relay.

### Luồng đăng ký hiện tại

```text
CHILD đăng nhập
→ childUsername đã xác thực được truyền qua AuthService, AuthController và AppController
→ ScreenStreamSender mở kết nối TCP đến Relay
→ CHILD gửi REGISTER_HOST
→ RelayServerHandler kiểm tra gói và yêu cầu RelayRegistry đăng ký
→ Relay gửi REGISTER_ACK
→ ScreenStreamSender xác nhận đăng ký được chấp nhận hoặc bị từ chối
```

### Hợp đồng wire của REGISTER_HOST

Loại RDTP: `0x01`.

| Trường header | Giá trị |
|---|---:|
| `flags` | `0` |
| `sessionId` | `0` |
| `sequenceNumber` | `0` |
| `payloadLength` | `2 + usernameLength` |

Payload dùng thứ tự byte **Big Endian**:

| Offset | Kiểu/kích thước | Nội dung |
|---|---|---|
| `0..1` | `uint16`, Big Endian | `usernameLength` |
| `2..` | `usernameLength` byte | `childUsername` mã hóa UTF-8 |

`usernameLength` phải nằm trong khoảng từ **1 đến 200 byte UTF-8**. Kích thước
payload phải đúng bằng `2 + usernameLength`.

### Hợp đồng wire của REGISTER_ACK

Loại RDTP: `0x02`.

| Trường header | Giá trị |
|---|---:|
| `flags` | `0` |
| `sessionId` | `0` |
| `sequenceNumber` | `0` |
| `payloadLength` | `1` |

Payload có đúng một byte:

| Giá trị | Kết quả |
|---:|---|
| `0` | Đăng ký bị từ chối |
| `1` | Đăng ký được chấp nhận |

---

## Nhom: Session Establishment

| Ten | Hex | Y nghia du kien |
|---|---|---|
| `CONNECT_REQUEST` | `0x03` | Client (viewer) gui den Relay de yeu cau ket noi voi mot Host cu the. |
| `SESSION_REQUEST` | `0x04` | Relay chuyen tiep yeu cau ket noi tu Client den Host. |
| `SESSION_ACCEPT` | `0x05` | Host chap nhan session duoc yeu cau. |
| `SESSION_REJECT` | `0x06` | Host tu choi session. |
| `CONNECT_RESULT` | `0x07` | Relay tra ket qua cuoi cung (accept/reject) ve cho Client. |

> **Payload:** Chua duoc dinh nghia/implement o Phase hien tai.

---

## Nhom: Screen (Media Stream)

| Ten | Hex | Y nghia du kien |
|---|---|---|
| `SCREEN_FRAME` | `0x10` | Mot khung hinh man hinh da duoc encode, truyen tu Host den Client. |

> **Payload:** Chua duoc dinh nghia/implement o Phase hien tai.

---

## Nhom: Mouse (Remote Input)

| Ten | Hex | Y nghia du kien |
|---|---|---|
| `MOUSE_MOVE` | `0x20` | Di chuyen con tro chuot (toa do tuyet doi hoac tuong doi). |
| `MOUSE_BUTTON_DOWN` | `0x21` | Nut chuot duoc nhan xuong. |
| `MOUSE_BUTTON_UP` | `0x22` | Nut chuot duoc tha ra. |
| `MOUSE_WHEEL` | `0x23` | Cuon banh xe chuot (scroll wheel delta). |

> **Payload:** Chua duoc dinh nghia/implement o Phase hien tai.

---

## Nhom: Keyboard (Remote Input)

| Ten | Hex | Y nghia du kien |
|---|---|---|
| `KEY_PRESS` | `0x30` | Phim ban phim duoc nhan xuong. |
| `KEY_RELEASE` | `0x31` | Phim ban phim duoc tha ra. |

> **Payload:** Chua duoc dinh nghia/implement o Phase hien tai.

---

## Nhom: Connection Health

| Ten | Hex | Y nghia du kien |
|---|---|---|
| `HEARTBEAT` | `0x40` | Packet keep-alive dinh ky. Ben gui khong mong nhan reply. |
| `PING` | `0x41` | Probe do round-trip latency. Ben gui mong nhan `PONG`. |
| `PONG` | `0x42` | Reply cho `PING`. |

> **Payload:** Chua duoc dinh nghia/implement o Phase hien tai.

---

## Nhom: Monitoring / Diagnostics

| Ten | Hex | Y nghia du kien |
|---|---|---|
| `TELEMETRY` | `0x50` | Thong ke bandwidth, frame rate, va cac so lieu hieu nang khac. |

> **Payload:** Chua duoc dinh nghia/implement o Phase hien tai.

---

## Nhom: Connection Management

| Ten | Hex | Y nghia du kien |
|---|---|---|
| `DISCONNECT` | `0x60` | Mot ben ket thuc session mot cach graceful (co thong bao). |
| `ERROR` | `0x70` | Thong bao loi cap protocol giua cac ben. |

> **Payload:** Chua duoc dinh nghia/implement o Phase hien tai.

---

## Bang tong hop tat ca MessageType

| Ten | Hex | Nhom |
|---|---|---|
| `REGISTER_HOST` | `0x01` | Host Registration |
| `REGISTER_ACK` | `0x02` | Host Registration |
| `CONNECT_REQUEST` | `0x03` | Session Establishment |
| `SESSION_REQUEST` | `0x04` | Session Establishment |
| `SESSION_ACCEPT` | `0x05` | Session Establishment |
| `SESSION_REJECT` | `0x06` | Session Establishment |
| `CONNECT_RESULT` | `0x07` | Session Establishment |
| `SCREEN_FRAME` | `0x10` | Screen |
| `MOUSE_MOVE` | `0x20` | Mouse |
| `MOUSE_BUTTON_DOWN` | `0x21` | Mouse |
| `MOUSE_BUTTON_UP` | `0x22` | Mouse |
| `MOUSE_WHEEL` | `0x23` | Mouse |
| `KEY_PRESS` | `0x30` | Keyboard |
| `KEY_RELEASE` | `0x31` | Keyboard |
| `HEARTBEAT` | `0x40` | Connection Health |
| `PING` | `0x41` | Connection Health |
| `PONG` | `0x42` | Connection Health |
| `TELEMETRY` | `0x50` | Monitoring |
| `DISCONNECT` | `0x60` | Connection Management |
| `ERROR` | `0x70` | Connection Management |

---

## MessageType tren wire

`MessageType` duoc serialize thanh 1 byte bang cach lay underlying `uint8_t` value.
Vi du: `MessageType::PING` (= `0x41`) tren wire la byte `41`.
Chi tiet xem: [06_HEADER_SERIALIZATION.md - MessageType Serialization](06_HEADER_SERIALIZATION.md)

---

## Tai lieu lien quan

- [02_PROTOCOL_CONSTANTS.md](02_PROTOCOL_CONSTANTS.md) - Cac hang so khac (MAGIC, VERSION, ...)
- [04_PROTOCOL_HEADER_MODEL.md](04_PROTOCOL_HEADER_MODEL.md) - Field `type` trong ProtocolHeader
