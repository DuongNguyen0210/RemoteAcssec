# 01 - RDTP Protocol Overview

> Xem them: [README.md](README.md)

---

## Phase 1A la gi?

Phase 1A la buoc dau tien trong qua trinh xay dung lop mang (networking layer) cua he thong RemoteAccess - mot ung dung dieu khien may tinh tu xa. Muc tieu cua Phase 1A la dat nen mong cho **wire protocol** - tuc la quy dinh cach du lieu duoc dong goi va nhan dang khi truyen qua mang.

Phase 1A **khong** implement bat ky ket noi TCP, gui/nhan du lieu thuc te, hay xu ly session nao. No chi dinh nghia cac hang so, kieu du lieu, va cau truc du lieu (data model) lam nen tang cho cac phase sau.

---

## Cac khai niem co ban

| Khai niem | Giai thich trong project nay |
|---|---|
| **Protocol** | Tap hop cac quy tac quy dinh cach Host, Client va Relay trao doi du lieu voi nhau qua mang. Protocol nay co ten la **RDTP** (Remote Desktop Transfer Protocol). |
| **Packet** | Mot don vi du lieu duoc gui di. Moi packet gom hai phan: **header** va **payload**. |
| **Header** | Phan dau co dinh cua moi packet, chua thong tin dieu khien: loai message, kich thuoc payload, session ID... Kich thuoc wire format cua header la **24 bytes**. |
| **MessageType** | Mot gia tri `uint8_t` trong header cho biet packet nay mang y nghia gi - vi du: yeu cau ket noi, khung hinh man hinh, su kien ban phim... |
| **Payload** | Phan du lieu thuc te di sau header. Noi dung payload phu thuoc vao `MessageType`. Payload chua duoc dinh nghia o Phase 1A. |
| **Wire format** | Bieu dien nhi phan thuc su duoc truyen qua mang. Khac voi C++ object trong bo nho (co the co padding/alignment). |

---

## Moi quan he tong quat

```
  Host (may bi dieu khien)
  Client (may dieu khien / viewer)
  Relay (may chu trung gian)
           |
           | giao tiep qua TCP (chua implement)
           | theo wire protocol RDTP
           v
       Protocol Layer
           |
           +-- ProtocolConstants.h  <-- hang so + MessageType
           |
           +-- ProtocolHeader.h     <-- data model cho 24-byte header
                    |
                    | serializeHeader()
                    v
           ProtocolSerializer       <-- da implement (Phase 1A.2B)
                    |
                    v
           [TCP network send]       <-- CHUA implement
```

---

## Luong su dung hien tai

```
ProtocolConstants.h
        |
        +-- PROTOCOL_MAGIC, PROTOCOL_VERSION, HEADER_SIZE, MAX_PAYLOAD_LENGTH
        +-- enum class MessageType : uint8_t
                 |
                 v
         ProtocolHeader.h
                 |
                 +-- struct ProtocolHeader { ... }
                 +-- explicit ProtocolHeader(MessageType)
                              |
                              v
                 ProtocolSerializer.h / .cpp
                              |
                              +-- serializeHeader() --> QByteArray (24 bytes)
                              |
                              v
                 [future TCP send]          <-- CHUA implement
```

`ProtocolSerializer::serializeHeader()` da duoc implement. Buoc tiep theo (gui QByteArray qua TCP) chua thuoc Phase hien tai.

---

## Tai lieu lien quan

- [02_PROTOCOL_CONSTANTS.md](02_PROTOCOL_CONSTANTS.md) - Chi tiet cac hang so
- [03_MESSAGE_TYPES.md](03_MESSAGE_TYPES.md) - Bang MessageType day du
- [04_PROTOCOL_HEADER_MODEL.md](04_PROTOCOL_HEADER_MODEL.md) - struct ProtocolHeader
- [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md) - 24-byte wire layout
- [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) - ProtocolSerializer
- [09_IMPLEMENTATION_STATUS.md](09_IMPLEMENTATION_STATUS.md) - Trang thai implementation