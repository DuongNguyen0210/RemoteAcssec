# 06 - Header Serialization (ProtocolSerializer)

> Files:
> - `clients/RemoteAccessApp/src/Network/Protocol/ProtocolSerializer.h`
> - `clients/RemoteAccessApp/src/Network/Protocol/ProtocolSerializer.cpp`
> Namespace: `Protocol`
> Phase: 1A.2B
> Xem them: [README.md](README.md)

---

## Muc tieu

Phase 1A.2A tao ra `ProtocolHeader` chi la mot C++ struct trong RAM. Phase 1A.2B thuc hien buoc tiep theo: chuyen `ProtocolHeader` object thanh chinh xac **24 bytes** theo RDTP wire format.

```
ProtocolHeader object (RAM)
        |
        | serializeHeader()
        v
24-byte RDTP wire header (QByteArray)
        |
        v
[future TCP send]  <-- CHUA implement
```

---

## File: `ProtocolSerializer.h`

File header khai bao class `ProtocolSerializer` trong `namespace Protocol`. No `#include` ca `QByteArray` (kieu du lieu tra ve) va `ProtocolHeader.h` (kieu tham so dau vao).

### Class `ProtocolSerializer`

```cpp
namespace Protocol {

class ProtocolSerializer
{
public:
    static QByteArray serializeHeader(const ProtocolHeader &header);
};

} // namespace Protocol
```

**`static`**: Ham la thanh vien tinh cua class, khong can tao object `ProtocolSerializer` de goi. Caller su dung truc tiep:

```cpp
QByteArray bytes = Protocol::ProtocolSerializer::serializeHeader(header);
```

**`const ProtocolHeader &header`**: Ham nhan `header` qua const reference. Khong copy object, chi doc gia tri. `const` dam bao ham khong duoc phep sua doi `header`.

**`QByteArray`**: Kieu chuoi bytes cua Qt. Ham tra ve mot `QByteArray` moi chua chinh xac 24 bytes la wire representation cua `header`.

---

## File: `ProtocolSerializer.cpp`

File implement chua hai phan: cac helper noi bo (file-scope `static`) va ham chinh `serializeHeader`.

### Ham chinh: `serializeHeader`

```cpp
QByteArray ProtocolSerializer::serializeHeader(const ProtocolHeader &header)
{
    QByteArray buf;
    buf.reserve(static_cast<int>(HEADER_SIZE));

    appendUInt32BE(buf, header.magic);                       // offset  0-3
    appendUInt8   (buf, header.version);                     // offset  4
    appendUInt8   (buf, static_cast<uint8_t>(header.type)); // offset  5
    appendUInt16BE(buf, header.flags);                       // offset  6-7
    appendUInt32BE(buf, header.payloadLength);               // offset  8-11
    appendUInt64BE(buf, header.sessionId);                   // offset 12-19
    appendUInt32BE(buf, header.sequenceNumber);              // offset 20-23

    Q_ASSERT(buf.size() == static_cast<int>(HEADER_SIZE));
    return buf;
}
```

Moi field duoc ghi theo dung thu tu wire format, tuyen tinh tu offset 0 den offset 23.

---

## MessageType Serialization

`MessageType` la `enum class : uint8_t`. Khi serialize, lay underlying `uint8_t` value:

```cpp
appendUInt8(buf, static_cast<uint8_t>(header.type));
```

Vi du: `MessageType::PING` co gia tri `0x41`. Tren wire, field `type` chiem dung **1 byte**:

```
41
```

Khong gui ten string "PING". Chi gui byte `0x41`. Ben nhan doc byte nay, tra cuu trong `MessageType` enum de biet loai message.

---

## QByteArray va Q_ASSERT

```cpp
QByteArray buf;
buf.reserve(static_cast<int>(HEADER_SIZE));  // pre-allocate capacity cho 24 bytes
```

**`reserve(HEADER_SIZE)`**: Cap phat truoc bo nho noi bo du cho 24 bytes. Day la toi uu hoa hieu nang, tranh QByteArray phai re-allocate nhieu lan khi append. Luu y: `reserve` khong lam `buf.size()` bang 24 ngay -- buf van rong (`size() == 0`). Chi sau khi 24 byte duoc `append` xong thi `size()` moi bang 24.

**`buf.append(static_cast<char>(...))`**: Moi lan goi `append` them mot byte vao cuoi buf. `static_cast<char>` chuyen kieu integer ve `char` de QByteArray chap nhan.

**`Q_ASSERT(buf.size() == static_cast<int>(HEADER_SIZE))`**: Kiem tra debug. Neu code co bug lam thieu hoac du byte, assert se bat loi ngay trong debug build.

---

## Tai sao KHONG serialize raw struct

Xem ly giai day du tai: [05_WIRE_HEADER_FORMAT.md - Tai sao KHONG serialize raw struct](05_WIRE_HEADER_FORMAT.md)

Tom tat: padding, alignment, byte order, portability - tat ca deu khien `memcpy(buf, &header, sizeof(header))` cho ket qua sai tren moi truong thuc.

---

## Test Vector - Vi du hoan chinh

Header mau:

```
magic          = 0x52445450
version        = 0x01
type           = MessageType::PING = 0x41
flags          = 0x1234
payloadLength  = 0x01020304
sessionId      = 0x0102030405060708
sequenceNumber = 0x11223344
```

Ket qua `serializeHeader()` (24 bytes, Big Endian):

```
Offset   Bytes               Field
------   ------------------  ------------------
 0- 3    52 44 54 50         magic ("RDTP")
    4    01                  version
    5    41                  type (PING)
 6- 7    12 34               flags
 8-11    01 02 03 04         payloadLength
12-15    01 02 03 04         sessionId (high 4 bytes)
16-19    05 06 07 08         sessionId (low 4 bytes)
20-23    11 22 33 44         sequenceNumber
------   ------------------
Total  = 24 bytes
```

Test vector nay da duoc xac nhan voi Python `struct.pack(">...")`: ket qua PASS.

---

## Tai lieu lien quan

- [04_PROTOCOL_HEADER_MODEL.md](04_PROTOCOL_HEADER_MODEL.md) - struct ProtocolHeader (dau vao)
- [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md) - Wire layout spec
- [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md) - Ly thuyet va chi tiet cac helper appendUIntXXBE
- [08_CMAKE_INTEGRATION.md](08_CMAKE_INTEGRATION.md) - CMake build setup