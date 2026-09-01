# 03 - MessageType

> Định nghĩa trong: `clients/RemoteAccessApp/src/Network/Protocol/protocolconstants.h`
> Namespace: `Protocol`
> Xem thêm: [README.md](README.md)

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
`childUsername`. Ánh xạ này hiện được Giai đoạn 3B dùng để tìm CHILD đích khi
ADMIN yêu cầu tạo phiên; bản thân `REGISTER_HOST` không tạo phiên.

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

## Nhóm: Thiết lập phiên

| Tên | Hex | Ý nghĩa hiện tại |
|---|---|---|
| `CONNECT_REQUEST` | `0x03` | ADMIN yêu cầu Relay tạo phiên với một `childUsername` cụ thể. |
| `SESSION_REQUEST` | `0x04` | Relay chuyển yêu cầu cùng `sessionId` do Relay cấp đến CHILD đã đăng ký. |
| `SESSION_ACCEPT` | `0x05` | CHILD chấp nhận phiên đang chờ. |
| `SESSION_REJECT` | `0x06` | CHILD từ chối phiên đang chờ. |
| `CONNECT_RESULT` | `0x07` | Relay trả kết quả cuối cùng cho ADMIN. |

### Luồng thiết lập phiên hiện tại

```text
ADMIN chọn childUsername thật
→ AdminSessionController kết nối hoặc tái sử dụng RelayClient
→ CONNECT_REQUEST(childUsername)
→ RelayRegistry tìm CHILD đã đăng ký và cấp sessionId khác 0
→ Relay tạo phiên PENDING
→ Relay gửi SESSION_REQUEST(sessionId) đến CHILD
→ CHILD kiểm tra và gửi SESSION_ACCEPT(sessionId)
→ Relay kiểm tra đúng nguồn CHILD và đúng phiên
→ Relay chuyển phiên thành ACTIVE
→ Relay gửi CONNECT_RESULT(thành công, sessionId) đến ADMIN
→ ADMIN kiểm tra kết quả và lưu active sessionId
```

### Hợp đồng wire của CONNECT_REQUEST

Loại RDTP: `0x03`.

| Trường header | Giá trị |
|---|---:|
| `flags` | `0` |
| `sessionId` | `0` |
| `sequenceNumber` | `0` |
| `payloadLength` | `2 + targetUsernameLength` |

Payload dùng thứ tự byte **Big Endian**:

| Offset | Kiểu/kích thước | Nội dung |
|---|---|---|
| `0..1` | `uint16`, Big Endian | `targetUsernameLength` |
| `2..` | `targetUsernameLength` byte | `targetChildUsername` mã hóa UTF-8 |

`targetUsernameLength` phải nằm trong khoảng **1 đến 200 byte UTF-8** và kích
thước payload phải đúng bằng `2 + targetUsernameLength`.

Relay trả kết quả thất bại nếu header hoặc payload không đúng hợp đồng, kênh
gửi đã đăng ký là CHILD, CHILD đích chưa đăng ký, hoặc ADMIN/CHILD đang có một
phiên `PENDING` hay `ACTIVE`.

### Hợp đồng wire của SESSION_REQUEST

Loại RDTP: `0x04`.

| Trường header | Giá trị |
|---|---:|
| `flags` | `0` |
| `sessionId` | Giá trị khác `0` do Relay cấp |
| `sequenceNumber` | `0` |
| `payloadLength` | `0` |

Thông điệp không có payload. Relay tạo trạng thái `PENDING` trước khi gửi
`SESSION_REQUEST` đến CHILD.

### Hợp đồng wire của SESSION_ACCEPT và SESSION_REJECT

- `SESSION_ACCEPT`: loại RDTP `0x05`.
- `SESSION_REJECT`: loại RDTP `0x06`.

Cả hai dùng cùng hợp đồng header:

| Trường header | Giá trị |
|---|---:|
| `flags` | `0` |
| `sessionId` | Cùng giá trị khác `0` trong `SESSION_REQUEST` |
| `sequenceNumber` | `0` |
| `payloadLength` | `0` |

Thông điệp không có payload. Relay chỉ xử lý khi phiên tồn tại, đang ở trạng
thái `PENDING`, nguồn là chính xác channel CHILD của phiên và `sessionId` khớp.

- Với `SESSION_ACCEPT`, Relay chuyển phiên sang `ACTIVE`.
- Với `SESSION_REJECT`, Relay xóa phiên đang chờ và trả thất bại cho ADMIN.
- Thông điệp không hợp lệ không bao giờ kích hoạt phiên.

### Hợp đồng wire của CONNECT_RESULT

Loại RDTP: `0x07`.

| Trường header | Giá trị |
|---|---:|
| `flags` | `0` |
| `sequenceNumber` | `0` |
| `payloadLength` | `1` |

| Kết quả | `sessionId` | `payload[0]` |
|---|---:|---:|
| Thành công | Giá trị khác `0` đã được Relay cấp | `1` |
| Thất bại | `0` | `0` |

`AdminSessionController` kiểm tra đầy đủ header, byte kết quả và quan hệ giữa
kết quả với `sessionId` trước khi lưu phiên đang hoạt động.

### Trạng thái sau Giai đoạn 4

Handshake phiên của Giai đoạn 3B hiện là điều kiện để gửi màn hình: CHILD chỉ
tạo `SCREEN_FRAME` khi giữ một `sessionId` khác `0`, còn Relay chỉ chuyển tiếp
khi ID đó thuộc một phiên `ACTIVE` và nguồn là chính xác channel CHILD của
phiên. Hợp đồng chi tiết được mô tả trong nhóm Screen bên dưới.

---

## Nhóm: Màn hình

| Tên | Hex | Ý nghĩa hiện tại |
|---|---|---|
| `SCREEN_FRAME` | `0x10` | Một chunk của ảnh JPEG màn hình, gắn với phiên `ACTIVE` và được Relay chuyển trực tiếp từ CHILD đến ADMIN. |

### Hợp đồng header của SCREEN_FRAME

| Trường header | Giá trị/ngữ nghĩa hiện tại |
|---|---|
| `type` | `0x10` |
| `flags` | Giữ nguyên ngữ nghĩa hiện có; packetizer hiện đặt `0` |
| `sessionId` | ID khác `0` của phiên `ACTIVE` hiện tại ở CHILD |
| `sequenceNumber` | Giữ nguyên ngữ nghĩa chỉ số chunk hiện có |
| `payloadLength` | `16 + số byte JPEG trong chunk` |

Giai đoạn 4 chỉ thay đổi việc gắn phiên trong **RDTP header**. Định dạng payload
không thay đổi.

### Metadata payload của SCREEN_FRAME

Mọi số nguyên nhiều byte đều dùng **Big Endian**:

| Offset | Kiểu/kích thước | Nội dung |
|---|---|---|
| `0` | `uint32`, 4 byte | `frameId` |
| `4` | `uint32`, 4 byte | `chunkIndex` |
| `8` | `uint32`, 4 byte | `chunkCount` |
| `12` | `uint32`, 4 byte | `totalFrameSize` |
| `16` | Số byte còn lại | Dữ liệu JPEG của chunk |

Hỗ trợ nhiều chunk, kích thước chunk và các trường `frameId`, `chunkIndex`,
`chunkCount`, `totalFrameSize` giữ nguyên so với trước Giai đoạn 4.

### Điều kiện gửi ở CHILD

`ScreenStreamSender` sở hữu `m_currentSessionId`:

- Nếu `m_currentSessionId == 0`, CHILD không chụp màn hình, không mã hóa JPEG,
  không packetize và không gửi `SCREEN_FRAME`.
- Nếu `m_currentSessionId != 0`, CHILD giữ nguyên chu kỳ chụp, chất lượng JPEG,
  cách chia chunk, kết nối `RelayClient` bền và chính sách backpressure có giới
  hạn; ID hiện tại được truyền chính xác vào `ScreenFramePacketizer`.

`ScreenFramePacketizer` không sở hữu hoặc cấp phát trạng thái phiên. Caller cung
cấp `sessionId`, packetizer chỉ ghi đúng giá trị đó vào `header.sessionId` của
từng chunk.

### Phân quyền định tuyến tại Relay

Relay gọi
`RelayRegistry.findActiveSessionForChild(sessionId, childChannel)` và chỉ chuyển
tiếp khi đồng thời thỏa mãn:

- `sessionId` khác `0`;
- phiên tồn tại;
- phiên ở trạng thái `ACTIVE`;
- channel nguồn là chính xác channel CHILD được gắn với phiên.

Relay từ chối mà không chuyển tiếp đối với ID bằng `0`, ID không tồn tại hoặc đã
cũ, phiên `PENDING`, nguồn ADMIN hoặc một CHILD khác. Vì vậy CHILD không thể định
tuyến khung hình chỉ bằng cách đoán `sessionId`; cả ràng buộc phiên và channel
nguồn đều bắt buộc.

### Chuyển tiếp trực tiếp

```text
Protocol SCREEN_FRAME đầu vào
→ kiểm tra sessionId, trạng thái ACTIVE và channel nguồn
→ ghi chính đối tượng Protocol gốc đến channel ADMIN
→ ScreenFrameHandler/ScreenFrameReassembler chẩn đoán có thể chạy sau đó
```

Mỗi chunk gốc được chuyển độc lập. Relay không ghép ảnh trước khi định tuyến,
không packetize lại, không sửa byte JPEG, metadata payload, `sequenceNumber` hay
`sessionId`. Bộ ghép khung phía Relay chỉ phục vụ chẩn đoán và không phải điều
kiện của đường chuyển tiếp.

ADMIN hiện nhận được byte TCP/RDTP đã chuyển tiếp, nhưng chưa có bộ ghép
`SCREEN_FRAME`, giải mã JPEG, tạo `QImage` hoặc hiển thị bằng Qt.

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
