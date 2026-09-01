# 02 - Protocol Constants

> File: `clients/RemoteAccessApp/src/Network/Protocol/protocolconstants.h`
> Namespace: `Protocol`
> Xem them: [README.md](README.md)

---

## Muc tieu

Phase 1A.1 tao ra mot diem chua duy nhat (single source of truth) cho toan bo hang so va kieu du lieu cua protocol. Thay vi moi phan code tu khai bao cac magic number hay gia tri phien ban, tat ca dung chung tu file nay, dam bao tinh nhat quan.

File `protocolconstants.h` la header-only (chi gom `.h`, khong co `.cpp`), thuoc `namespace Protocol`. Tat ca Host, Client va Relay (khi duoc implement) deu se `#include` file nay.

---

## PROTOCOL_MAGIC

```cpp
inline constexpr uint32_t PROTOCOL_MAGIC = 0x52445450U;
```

| Thuoc tinh | Gia tri |
|---|---|
| **Kieu** | `uint32_t` (so nguyen khong dau 32-bit) |
| **Gia tri** | `0x52445450` |
| **Y nghia** | "Magic number" - 4 byte dau tien cua moi packet |

**Tai sao can magic number?**

Khi mot ben nhan du lieu qua mang, can cach nhanh de kiem tra: "Day co phai la packet hop le cua RDTP khong, hay chi la nhieu hoac du lieu sai?" Neu 4 byte dau cua packet khong bang `0x52445450`, packet do bi tu choi ngay.

**Giai ma gia tri:**

```
0x52 = 'R'
0x44 = 'D'
0x54 = 'T'
0x50 = 'P'
```

Bon byte ghep lai tao thanh chuoi **"RDTP"** - viet tat cua *Remote Desktop Transfer Protocol*.

---

## PROTOCOL_VERSION

```cpp
inline constexpr uint8_t PROTOCOL_VERSION = 1U;
```

| Thuoc tinh | Gia tri |
|---|---|
| **Kieu** | `uint8_t` (so nguyen khong dau 8-bit) |
| **Gia tri** | `1` |
| **Y nghia** | Phien ban hien tai cua wire protocol |

**Tai sao can version?**

Trong qua trinh phat trien, protocol co the thay doi (them field moi, doi cach encode...). Khi hai ben ket noi, chung co the kiem tra version de phat hien xem minh co "noi chung ngon ngu" khong. Neu version khong khop, ket noi co the bi tu choi hoac can negotiate. Hien tai version la `1`.

---

## HEADER_SIZE

```cpp
inline constexpr uint32_t HEADER_SIZE = 24U;
```

| Thuoc tinh | Gia tri |
|---|---|
| **Kieu** | `uint32_t` |
| **Gia tri** | `24` |
| **Y nghia** | Kich thuoc header trong **wire format** (tren day truyen) |

**Quan trong: HEADER_SIZE KHAC `sizeof(ProtocolHeader)`**

`HEADER_SIZE = 24` la kich thuoc cua header khi duoc **serialize ra bytes** va truyen di:

```
magic           4 bytes
version         1 byte
type            1 byte
flags           2 bytes
payloadLength   4 bytes
sessionId       8 bytes
sequenceNumber  4 bytes
------------------------
Tong           24 bytes
```

Trong khi do, `sizeof(ProtocolHeader)` - kich thuoc cua C++ struct trong bo nho - **co the lon hon 24** do:

- **Padding:** Compiler tu them byte dem de can chinh dia chi bo nho (alignment)
- **Alignment:** Vi du, `uint64_t sessionId` thuong yeu cau dia chi chia het cho 8

Do do, code **khong duoc** dung `memcpy` hoac `reinterpret_cast` de gui struct nay thang qua mang.
Xem them: [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md)

---

## MAX_PAYLOAD_LENGTH

```cpp
inline constexpr uint32_t MAX_PAYLOAD_LENGTH = 524288U; // 512 * 1024
```

| Thuoc tinh | Gia tri |
|---|---|
| **Kieu** | `uint32_t` |
| **Gia tri** | `524288` bytes = **512 KiB** |
| **Y nghia** | Kich thuoc toi da payload trong mot packet |

**Tai sao can gioi han?**

- Ngan mot ben gui packet khong lo lam tran bo nho ben nhan
- Giup receiver phan bo buffer co kich thuoc co the du doan
- Phat hien packet bi loi: neu `payloadLength` doc duoc lon hon 512 KiB thi packet do bat thuong

---

## Tai lieu lien quan

- [03_MESSAGE_TYPES.md](03_MESSAGE_TYPES.md) - MessageType enum (cung dinh nghia trong cung file)
- [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md) - Chi tiet wire layout 24 bytes
