# 04 - ProtocolHeader Data Model

> File: `clients/RemoteAccessApp/src/Network/Protocol/protocolheader.h`
> Namespace: `Protocol`
> Phase: 1A.2A
> Xem them: [README.md](README.md)

---

## Muc tieu

Phase 1A.2A tao ra **data model** - mot C++ struct - dai dien cho phan header chung cua moi packet trong protocol RDTP.

**Day chi la data model.** Phase nay khong implement serialization, khong gui du lieu, khong doc tu mang. Serialization thuoc [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md).

---

## Tai sao moi packet can common header?

Khi mot ben nhan du lieu tu mang, buoc dau tien luon la doc **header** de biet:

1. Day co phai packet RDTP hop le khong? (kiem tra `magic`)
2. Day la packet loai gi? (doc `type` - xac dinh `MessageType`)
3. Payload dai bao nhieu byte? (doc `payloadLength`)
4. Packet nay thuoc session nao? (doc `sessionId`)

Header chung giup ben nhan xu ly phan dieu khien truoc khi biet payload cu the la gi.

---

## Struct definition (tu code thuc te)

```cpp
struct ProtocolHeader {
    uint32_t magic;
    uint8_t  version;
    MessageType type;
    uint16_t flags;
    uint32_t payloadLength;
    uint64_t sessionId;
    uint32_t sequenceNumber;

    explicit ProtocolHeader(MessageType messageType)
        : magic          { PROTOCOL_MAGIC }
        , version        { PROTOCOL_VERSION }
        , type           { messageType }
        , flags          { 0 }
        , payloadLength  { 0 }
        , sessionId      { 0 }
        , sequenceNumber { 0 }
    {}
};
```

---

## Giai thich tung field

### `magic` - `uint32_t`

- Chua gia tri `PROTOCOL_MAGIC` (`0x52445450` = "RDTP")
- Mac dinh: `PROTOCOL_MAGIC`
- Dung de nhan dien protocol: ben nhan kiem tra field nay truoc tien. Neu khong khop => packet khong hop le, bo qua hoac dong ket noi.

### `version` - `uint8_t`

- Chua phien ban wire protocol
- Mac dinh: `PROTOCOL_VERSION` (hien tai = `1`)
- Cho phep phat hien version mismatch khi hai ben co phien ban protocol khac nhau.

### `type` - `MessageType`

- Xac dinh loai message ma packet nay mang
- **Khong co gia tri default** - caller bat buoc phai truyen vao khi tao `ProtocolHeader`
- Vi du: mot packet `PING` va mot packet `SCREEN_FRAME` co cung cau truc header, nhung `type` khac nhau nen ben nhan biet cach xu ly payload khac nhau.

### `flags` - `uint16_t`

- Danh cho cac **bit flag** dieu khien protocol
- Mac dinh: `0`
- **CHUA DINH NGHIA** cac bit flag cu the o Phase hien tai. Field nay duoc danh san (reserved) cho cac phase sau.

### `payloadLength` - `uint32_t`

- So byte cua phan **payload** di ngay sau header
- **Khong** bao gom 24 byte cua header
- Mac dinh: `0` (khong co payload)
- Ben nhan doc field nay de biet can doc them bao nhieu byte tu network sau khi doc xong header.

### `sessionId` - `uint64_t`

- Dinh danh session (phien lam viec) ma packet nay thuoc ve
- Mac dinh: `0` (chua co session)
- Cho phep phan biet cac session dang chay song song qua cung mot Relay
- **Luu y:** Session management logic (tao session, quan ly session) **chua duoc implement**. `sessionId` chi la field chua gia tri.

### `sequenceNumber` - `uint32_t`

- So thu tu cua packet trong mot stream
- Mac dinh: `0`
- Du kien dung de: phat hien packet bi mat, sap xep lai packet den khong dung thu tu, deduplication
- **Luu y:** Logic su dung `sequenceNumber` **chua duoc implement**.

---

## Bang tong hop

| Field | Type | Default | Y nghia |
|---|---|---|---|
| `magic` | `uint32_t` | `PROTOCOL_MAGIC` (0x52445450) | Nhan dien packet RDTP hop le |
| `version` | `uint8_t` | `PROTOCOL_VERSION` (1) | Phien ban protocol |
| `type` | `MessageType` | *(bat buoc truyen vao)* | Loai message cua packet |
| `flags` | `uint16_t` | `0` | Bit flags - CHUA DINH NGHIA |
| `payloadLength` | `uint32_t` | `0` | So byte payload sau header |
| `sessionId` | `uint64_t` | `0` | ID session (logic chua implement) |
| `sequenceNumber` | `uint32_t` | `0` | So thu tu packet (logic chua implement) |

---

## Constructor

### Signature

```cpp
explicit ProtocolHeader(MessageType messageType)
```

### Giai thich

**`ProtocolHeader(MessageType messageType)`**: Constructor nhan duy nhat mot tham so. Caller phai truyen `MessageType` khi tao object.

**Tu khoa `explicit`**: Ngan C++ tu dong chuyen doi ngam dinh (implicit conversion):

```cpp
// Neu khong co explicit - se bi ngan boi explicit:
void sendHeader(ProtocolHeader h);
sendHeader(MessageType::PING);  // Compiler bao loi - dung nhu mong muon
```

Voi `explicit`, caller phai viet ro rang:

```cpp
sendHeader(ProtocolHeader(MessageType::PING));  // OK - ro rang
```

**Tai sao bat caller truyen `MessageType`?** Moi packet phai co mot loai message xac dinh ngay tu dau. Khong co packet nao hop le ma khong co `type`. Neu cho phep tao `ProtocolHeader` ma khong truyen `MessageType`, object do se o trang thai khong xac dinh.

**Initializer list** khoi tao tat ca fields ngay khi object duoc tao. Cu phap `{ value }` la uniform initialization (C++11), ngan narrowing conversion:

```cpp
: magic          { PROTOCOL_MAGIC }
, version        { PROTOCOL_VERSION }
, type           { messageType }
, flags          { 0 }
, payloadLength  { 0 }
, sessionId      { 0 }
, sequenceNumber { 0 }
```

---

## Vi du minh hoa (khai niem)

```cpp
// Tao header cho mot PING packet
ProtocolHeader header(MessageType::PING);

// Luc nay:
// header.magic          == 0x52445450  (PROTOCOL_MAGIC)
// header.version        == 1           (PROTOCOL_VERSION)
// header.type           == MessageType::PING
// header.flags          == 0
// header.payloadLength  == 0
// header.sessionId      == 0
// header.sequenceNumber == 0
```

> **Luu y:** Day chi la vi du minh hoa. Khong co code production nao gui object nay qua mang o Phase hien tai.

---

## Tai sao Phase nay chi la data model?

De gui `ProtocolHeader` qua TCP, can phai **serialize** no ra mot chuoi byte dung thu tu va dung byte order (big-endian theo convention mang). O Phase 1A.2A, chi can dam bao rang struct nay ton tai voi dung cac field, dung kieu, va dung gia tri default. Cac phase tiep theo se xay dung serialization tren nen nay.

Xem implementation: [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md)

---

## Tai lieu lien quan

- [02_PROTOCOL_CONSTANTS.md](02_PROTOCOL_CONSTANTS.md) - PROTOCOL_MAGIC, PROTOCOL_VERSION
- [03_MESSAGE_TYPES.md](03_MESSAGE_TYPES.md) - MessageType enum day du
- [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md) - 24-byte wire layout
- [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) - Chuyen struct nay thanh bytes
