# 08 - CMake Integration

> File: `clients/RemoteAccessApp/src/Network/CMakeLists.txt`
> Xem them: [README.md](README.md)

---

## Tong quan

Module `Network` duoc build thanh static library (`libNetwork.a`). CMakeLists.txt cua module nay khai bao tat ca source files va cau hinh include directories.

---

## NETWORK_SOURCES

```cmake
set(NETWORK_SOURCES dummy.cpp
    # Phase 1A.1 - wire-protocol foundation (header-only, listed for IDE indexing)
    Protocol/protocolconstants.h
    # Phase 1A.2A - wire-protocol header model (header-only, listed for IDE indexing)
    Protocol/protocolheader.h
    # Phase 1A.2B - header serialization
    Protocol/protocolserializer.h
    Protocol/protocolserializer.cpp
)

add_library(Network STATIC ${NETWORK_SOURCES})
```

**`NETWORK_SOURCES`** la bien CMake chua danh sach tat ca file thuoc target `Network`.

**`dummy.cpp`**: File C++ placeholder can thiet vi CMake khong cho phep tao thu vien STATIC hoan toan khong co file `.cpp`.

**Tai sao them file `.h` vao `NETWORK_SOURCES`?**

Cac file header-only (`protocolconstants.h`, `protocolheader.h`) khong can compile thanh `.o`, nhung duoc them vao de:
- **IDE indexing:** Visual Studio, CLion, Qt Creator co the hien thi chung trong project tree
- **Dependency tracking:** Mot so CMake generator (Ninja, Make) co the theo doi thay doi header

`protocolserializer.cpp` la file nguon thuc su duoc compile.

---

## target_include_directories

```cmake
target_include_directories(Network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
```

Khai bao thu muc goc `src/` cho target `Network`:

| Thu muc | Cho phep include bang cach nao |
|---|---|
| `${CMAKE_CURRENT_SOURCE_DIR}/..` (= `src/`) | `#include "Network/Protocol/protocolconstants.h"` |

Tu khoa `PUBLIC` co nghia la bat ky target nao `link` voi `Network` cung tu dong duoc ke thua cac include directories nay. Khong can khai bao lai o target khac.

---

## Qt Network dependency

```cmake
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)
target_link_libraries(Network PRIVATE Qt${QT_VERSION_MAJOR}::Network)
```

Module `Network` link voi Qt Network module (can thiet cho `QByteArray` va cac Qt networking types duoc dung trong `ProtocolSerializer`).

---

## Tai lieu lien quan

- [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) - ProtocolSerializer (files duoc them o Phase 1A.2B)
- [09_IMPLEMENTATION_STATUS.md](09_IMPLEMENTATION_STATUS.md) - Trang thai build
