# 05 - Wire Header Format (24 bytes)

> Xem them: [README.md](README.md)

---

## Layout logic

Day la **wire representation** - cach header duoc serialize thanh byte stream khi truyen qua mang. Day la spec, khong phai implementation chi tiet.

```
Offset  Size  Field
------------------------------------------
 0      4     magic           (uint32_t)
 4      1     version         (uint8_t)
 5      1     type            (uint8_t <- underlying type cua MessageType)
 6      2     flags           (uint16_t)
 8      4     payloadLength   (uint32_t)
12      8     sessionId       (uint64_t)
20      4     sequenceNumber  (uint32_t)
------------------------------------------
             24 bytes tong cong
```

Bang chi tiet:

| Offset | Size (bytes) | Field | Type |
|---|---|---|---|
| 0-3 | 4 | `magic` | `uint32_t` |
| 4 | 1 | `version` | `uint8_t` |
| 5 | 1 | `type` | `MessageType` (underlying `uint8_t`) |
| 6-7 | 2 | `flags` | `uint16_t` |
| 8-11 | 4 | `payloadLength` | `uint32_t` |
| 12-19 | 8 | `sessionId` | `uint64_t` |
| 20-23 | 4 | `sequenceNumber` | `uint32_t` |
| **Tong** | **24** | | |

24 bytes = 192 bits.

Tat ca integer nhieu hon 1 byte duoc ghi theo **Big Endian / Network Byte Order**.
Chi tiet ly thuyet byte order xem: [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md)

---

## HEADER_SIZE vs sizeof(ProtocolHeader)

`HEADER_SIZE = 24` la kich thuoc **wire format**. No khac voi `sizeof(ProtocolHeader)`:

- `sizeof(ProtocolHeader)` la kich thuoc C++ struct trong bo nho RAM
- C++ compiler co the chen **padding bytes** giua cac field de can chinh dia chi (alignment)
- Vi du: `uint64_t sessionId` thuong yeu cau dia chi chia het cho 8, gay compiler them 2 padding bytes sau `flags` (2 bytes), khien sizeof lon hon 24

Do do, `sizeof(ProtocolHeader)` co the la 28, 32, hoac gia tri khac, tuy compiler va kien truc CPU.

---

## Tai sao KHONG serialize raw struct

### Cac cach KHONG duoc dung

```cpp
// SAI - tuyet doi khong lam the nay:
memcpy(buf.data(), &header, sizeof(ProtocolHeader));
// hoac:
reinterpret_cast<ProtocolHeader*>(buf.data());
// hoac: #pragma pack(push, 1)
```

### Ly do

| Van de | Giai thich |
|---|---|
| **Padding** | Compiler chen byte dem giua cac field de can chinh bo nho. `sizeof(ProtocolHeader)` co the lon hon 24. Padding bytes se bi copy theo, gay sai offset. |
| **Alignment** | `uint64_t sessionId` can dia chi chia het cho 8, gay padding truoc no, lam lech offset cac field sau. |
| **Byte order** | Tren x86 (Little Endian), `uint32_t magic = 0x52445450` trong RAM co bytes `50 54 44 52`. Copy thang ra wire => ben nhan doc sai gia tri. |
| **Portability** | Code dung tren x86 nhung sai tren Big Endian CPU, hoac nguoc lai. |
| **`#pragma pack`** | Ep compiler bo padding nhung co the gay truy cap unaligned memory, crash hoac hieu nang kem tren mot so kien truc. |

### Giai phap dung

Serialization thu cong - ghi tung field mot cach tuong minh theo dung byte order. Day la cach da implement trong [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md).

---

## Tai lieu lien quan

- [02_PROTOCOL_CONSTANTS.md](02_PROTOCOL_CONSTANTS.md) - Giai thich HEADER_SIZE
- [04_PROTOCOL_HEADER_MODEL.md](04_PROTOCOL_HEADER_MODEL.md) - struct ProtocolHeader trong RAM
- [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) - Implementation serialization
- [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md) - Ly thuyet byte order