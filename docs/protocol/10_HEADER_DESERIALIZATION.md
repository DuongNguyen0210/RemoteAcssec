# 10 - Header Deserialization (ProtocolSerializer)

> Files:
> - `clients/RemoteAccessApp/src/Network/Protocol/ProtocolSerializer.h`
> - `clients/RemoteAccessApp/src/Network/Protocol/ProtocolSerializer.cpp`
> Namespace: `Protocol`
> Phase: 1A.2C
> Xem them: [README.md](README.md)

---

## 1. Muc tieu (Phase 1A.2C)

Phase 1A.2B thuc hien chieu **serialize**:

```
ProtocolHeader object
        ↓ serializeHeader()
24-byte RDTP wire header
```

Phase 1A.2C (Phase hien tai) lam chieu nguoc lai - **deserialize**:

```
24-byte RDTP wire header
        ↓ deserializeHeader()
ProtocolHeader object
```

**Giai thich:**
- Phase 1A.2C dung khi Client hoac Host nhan duoc mot header RDTP tu mang.
- No doc cac raw bytes va phuc hoi lai thanh cac field cua `ProtocolHeader` struct.
- Phase nay **CHUA** doc truc tiep tu mang (TCP).
- Phase nay gia dinh rang no duoc cung cap **dung 1 QByteArray** chua mot 24-byte header hoan chinh.

---

## 2. API deserializeHeader()

Ham deserialization duoc them vao class `ProtocolSerializer`:

```cpp
static std::optional<ProtocolHeader>
deserializeHeader(const QByteArray& data);
```

**Giai thich:**
- **`static`**: Ham tinh, goi thong qua `ProtocolSerializer::deserializeHeader(...)` ma khong can tao object.
- **`const QByteArray& data`**: Chuoi byte dau vao (raw data).
- **`std::optional<ProtocolHeader>`**: Tra ve ket qua.
  - Neu `data` hop le ve kich thuoc (dung 24 bytes), tra ve mot the hien `ProtocolHeader`.
  - Neu `data.size() != HEADER_SIZE`, tra ve `std::nullopt` (khong co gia tri).
- Su dung `std::optional` la cach an toan, hien dai de the hien su that bai ma khong can dung den C++ exceptions hoac con tro (pointer), vi neu truyen sai so byte (khong phai 24 bytes) thi khong the tao mot `ProtocolHeader` hop le ve mat cau truc.

---

## 3. Size Validation (Kiem tra kich thuoc)

Phan kiem tra duy nhat xay ra hien tai la do dai cua mang:

```cpp
if (data.size() != static_cast<int>(HEADER_SIZE)) {
    return std::nullopt;
}
```

Voi `HEADER_SIZE = 24 bytes`.

**LUU Y QUAN TRONG:**
Phase nay **chi deserialize mot header HOAN CHINH**.
- Dau vao 24 bytes → Xac nhan hop le va xu ly tiep.
- Dau vao 10 bytes → Tra ve `std::nullopt`.
- Dau vao 30 bytes → Tra ve `std::nullopt`.

Viec he thong mang nhan 10 bytes lan 1, roi nhan them 14 bytes lan 2 de ghep lai thanh 24 bytes **KHONG thuoc trach nhiem cua `deserializeHeader()`**. Cong viec (buffering/framing) do se thuoc ve **ProtocolParser** o phase sau.

---

## 4. Field Offsets

`deserializeHeader()` phai lay tung field ra dua vao bang offset cua giao thuc RDTP:

| Offset | Size (bytes) | Field | Kieu |
|---|---|---|---|
| 0-3 | 4 | `magic` | `uint32_t` |
| 4 | 1 | `version` | `uint8_t` |
| 5 | 1 | `type` | `MessageType` (`uint8_t`) |
| 6-7 | 2 | `flags` | `uint16_t` |
| 8-11 | 4 | `payloadLength` | `uint32_t` |
| 12-19 | 8 | `sessionId` | `uint64_t` |
| 20-23 | 4 | `sequenceNumber` | `uint32_t` |

*(Tong cong 24 bytes = 192 bits)*

Tham khao chi tiet: [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md)

---

## 5. Big Endian Deserialization (Network Byte Order)

Khi serialize (Phase 1A.2B), cac integer tren 1 byte da duoc ghi theo kieu **BIG ENDIAN / NETWORK BYTE ORDER** (byte cao luu truoc).

Khi deserialize, he thong phai doc nguoc lai de dam bao chinh xac gia tri (khong phu thuoc kien truc Little/Big Endian cua CPU hien tai).

Vi du:
- bytes: `12 34` → `uint16_t`: `0x1234`
- bytes: `12 34 56 78` → `uint32_t`: `0x12345678`
- bytes: `01 02 03 04 05 06 07 08` → `uint64_t`: `0x0102030405060708`

Tham khao nguyen ly Big Endian: [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md)

---

## 6. Cac Helper `read...BE()` va Signed Byte Handling

Implementation dung cac ham helper noi bo trong file `ProtocolSerializer.cpp`:
- `readUInt8(buf, offset)`
- `readUInt16BE(buf, offset)`
- `readUInt32BE(buf, offset)`
- `readUInt64BE(buf, offset)`

**Signed Byte Handling:**
Kieu `char` (duoc `QByteArray` su dung) tren nhieu platform la kieu co dau (signed). Mot raw byte co gia tri tren hoac bang `0x80` (vi du: `0xFF`) neu chuyen ep kieu thang vao integer lon hon co the bi *sign extension* (vi du `0xFF` thanh `0xFFFFFFFF`).
De phong tranh, implementation truoc tien luon ép ve `uint8_t` truoc khi shift hoac bitwise OR.

**Vi du phep doc `uint32_t` (Big Endian):**

```cpp
return
    (static_cast<uint32_t>(static_cast<uint8_t>(buf.at(offset    ))) << 24) |
    (static_cast<uint32_t>(static_cast<uint8_t>(buf.at(offset + 1))) << 16) |
    (static_cast<uint32_t>(static_cast<uint8_t>(buf.at(offset + 2))) <<  8) |
    (static_cast<uint32_t>(static_cast<uint8_t>(buf.at(offset + 3))) <<  0);
```

- Tu byte, lay ra the hien unsign `uint8_t`.
- Convert the hien `uint8_t` len `uint32_t`.
- Dich trai (`<<`) de day gia tri byte vao dung vi tri tuong ung (24, 16, 8, 0).
- Dung phep OR bitwise (`|`) de gop the hien cac bit tro lai voi nhau.

---

## 7. MessageType Decoding & Tao ProtocolHeader Object

Byte tai `offset 5` dai dien cho `MessageType`.

Tai thoi diem Phase 1A.2A, struct `ProtocolHeader` khong co constructor mac dinh, chi chap nhan:
`explicit ProtocolHeader(MessageType messageType)`

Do do, qua trinh tao object dien ra nhu sau:
1. Doc byte offset 5 va cast ve `MessageType`:
   ```cpp
   const uint8_t rawType = readUInt8(data, 5);
   const MessageType type = static_cast<MessageType>(rawType);
   ```
2. Goi constructor de tao object:
   ```cpp
   ProtocolHeader header(type);
   ```
3. Cap nhat cac field con lai (`magic`, `version`, `flags`, `payloadLength`, `sessionId`, `sequenceNumber`) tu `QByteArray` vao object `header`.

Khong them empty constructor (default constructor) cho viec deserialize nay de giu tinh kien dinh cua Phase 1A.2A.

Khong convert text chuoi vi du nhu: `"PING"`.

---

## 8. Khong dung Raw Struct Memory

TUYET DOI KHONG su dung:
- `memcpy()`
- `reinterpret_cast`
- `#pragma pack`
- `sizeof(ProtocolHeader)`

Wire format (truyen tai) va memory layout trong RAM cua C++ la hai thu khac nhau hoan toan. Dung raw direct read se dan toi sai padding, unaligned issue, byte-order hoac pointer error. Vay nen tung truong phai duoc trich xuat va noi voi nhau theo cac helper.

---

## 9. Validation chua thuoc Phase nay

Hien tai code **CHUA** kiem tra rang buoc logic nhu:
- `magic` co bang `PROTOCOL_MAGIC` (0x52445450) hay khong?
- `version` co nam trong cac phien ban duoc ung ho hay khong?
- `MessageType` co la ma enum ton tai thuc te khong?

Phase nay **chi chiu trach nhiem**:
*"Decode mot header co dung 24 bytes thanh ProtocolHeader."*

Kiem tra tinh hop ly cua header se phu thuoc vao lop quan ly giao thuc o phan tiep theo, day khong phai bug cua Phase 1A.2C.

---

## 10. Phan Biet Deserializer va Parser

Dieu vo cung quan trong la phan biet muc tieu cua file nay voi Parser:

**`deserializeHeader()` (Hien tai):**
- [24 bytes hoan chinh] → `ProtocolHeader`
- KHONG lam buffering (luu tam byte mang).
- KHONG xu periodic TCP fragmentation (phan manh tcp thanh tung manh mang).
- KHONG tim ranh gioi packet (goi the).

**`ProtocolParser` (Cac phase sau):**
- Doc duoc TCP stream dang: `10 bytes` + `14 bytes` + `100 bytes` payload + nhieu packet khac dinh nhau.
- Buffering va tach packet tu cac header da nhan.
- Cuoi cung boc 24 bytes roi dua vao cho `deserializeHeader()`.

---

## 11. Round-Trip Verification

Muc do verify da the hien (Code implementation test - PASS):

1. **ProtocolHeader** (Tao gia tri ban dau).
2. → `serializeHeader()` (Chuyen thanh 24 byte array).
3. → `deserializeHeader()` (Lay byte array ve object thuc the ProtocolHeader khac).
4. → Kiem tra xem 2 object truoc sau nay co giong nhau ve:
   `magic`, `version`, `type`, `flags`, `payloadLength`, `sessionId`, `sequenceNumber` hay khong?
   Ket qua cho the hien 1A.2C: **Tat ca cac field giong object ban dau**.
