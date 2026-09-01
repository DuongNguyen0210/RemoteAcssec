# 07 - Big Endian / Network Byte Order

> Xem them: [README.md](README.md)

---

## Big Endian la gi?

**Big Endian** la quy uoc byte co gia tri lon nhat (Most Significant Byte - MSB) duoc ghi truoc. Day la chuan cua network protocol ("network byte order").

Vi du: so `uint32_t` co gia tri `0x12345678`:

```
Byte 0 (offset thap nhat): 0x12  <- MSB
Byte 1:                    0x34
Byte 2:                    0x56
Byte 3 (offset cao nhat):  0x78  <- LSB
```

Tren wire: `12 34 56 78`

---

## Tai sao khong phu thuoc vao byte order cua CPU?

CPU x86/x64 su dung **Little Endian**: byte co gia tri nho nhat duoc luu truoc. Neu copy truc tiep bo nho cua struct ra buffer:

- Tren x86: `0x12345678` -> bytes `78 56 34 12` -> **SAI** wire format
- Tren Big Endian CPU: `0x12345678` -> bytes `12 34 56 78` -> dung

Neu dung `memcpy` hoac `reinterpret_cast`, code chi chay dung tren mot loai CPU. Serialization thu cong bang bit-shift (nhu implementation nay) dam bao ket qua giong nhau tren moi kien truc.

---

## Cac Helper: Giai thich Bit-Shift

Cac ham helper khai bao `static` trong `protocolserializer.cpp` (file-scope), khong duoc export ra ngoai. Chung chi phuc vu `serializeHeader`.

---

### `appendUInt16BE` - 2 bytes

```cpp
static void appendUInt16BE(QByteArray &buf, uint16_t value)
{
    buf.append(static_cast<char>((value >> 8) & 0xFF));  // byte cao (MSB)
    buf.append(static_cast<char>((value >> 0) & 0xFF));  // byte thap (LSB)
}
```

Vi du: `value = 0x1234`

| Phep tinh | Ket qua | Y nghia |
|---|---|---|
| `value >> 8` | `0x12` | Dich phai 8 bit, byte cao xuong vi tri thap nhat |
| `& 0xFF` | `0x12` | Giu lai 8-bit thap nhat |
| `value >> 0` | `0x1234` | Giu nguyen (shift 0) |
| `& 0xFF` | `0x34` | Lay byte thap (LSB) |

Ket qua append: `12 34`

---

### `appendUInt32BE` - 4 bytes

```cpp
static void appendUInt32BE(QByteArray &buf, uint32_t value)
{
    buf.append(static_cast<char>((value >> 24) & 0xFF));
    buf.append(static_cast<char>((value >> 16) & 0xFF));
    buf.append(static_cast<char>((value >>  8) & 0xFF));
    buf.append(static_cast<char>((value >>  0) & 0xFF));
}
```

Vi du: `value = 0x12345678`

| Shift | Ket qua | Byte |
|---|---|---|
| `>> 24` | `0x12` | Byte cao nhat (MSB) |
| `>> 16` | `0x34` | Byte thu 2 |
| `>> 8` | `0x56` | Byte thu 3 |
| `>> 0` | `0x78` | Byte thap nhat (LSB) |

Ket qua append: `12 34 56 78`

---

### `appendUInt64BE` - 8 bytes

```cpp
static void appendUInt64BE(QByteArray &buf, uint64_t value)
{
    buf.append(static_cast<char>((value >> 56) & 0xFF));
    buf.append(static_cast<char>((value >> 48) & 0xFF));
    buf.append(static_cast<char>((value >> 40) & 0xFF));
    buf.append(static_cast<char>((value >> 32) & 0xFF));
    buf.append(static_cast<char>((value >> 24) & 0xFF));
    buf.append(static_cast<char>((value >> 16) & 0xFF));
    buf.append(static_cast<char>((value >>  8) & 0xFF));
    buf.append(static_cast<char>((value >>  0) & 0xFF));
}
```

Vi du: `value = 0x0102030405060708`

| Shift | Ket qua | Byte |
|---|---|---|
| `>> 56` | `0x01` | Byte cao nhat (MSB) |
| `>> 48` | `0x02` | ... |
| `>> 40` | `0x03` | ... |
| `>> 32` | `0x04` | ... |
| `>> 24` | `0x05` | ... |
| `>> 16` | `0x06` | ... |
| `>> 8` | `0x07` | ... |
| `>> 0` | `0x08` | Byte thap nhat (LSB) |

Ket qua append: `01 02 03 04 05 06 07 08`

---

## Giai thich toan tu

| Toan tu | Y nghia |
|---|---|
| `>>` (right shift) | Dich bit sang phai. `value >> N` chuyen byte can lay xuong vi tri thap nhat de co the lay qua `& 0xFF`. |
| `& 0xFF` | Bit mask. Giu lai dung 8-bit thap nhat, loai bo phan cao hon. Can thiet vi sau khi shift, bien van co the rong hon 8 bit. |
| `>> 0` | Shift 0 bit - giu nguyen gia tri. Dung de style nhat quan voi cac dong shift khac trong cung ham. |
| MSB truoc LSB | Byte cao (Most Significant Byte) duoc `append` truoc, byte thap (Least Significant Byte) sau. Day chinh la dinh nghia Big Endian. |

---

## Tai lieu lien quan

- [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md) - Wire layout va ly do khong dung raw copy
- [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) - serializeHeader() su dung cac helper nay
