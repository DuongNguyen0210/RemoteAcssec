# 11 - Phase 1A.2D: ProtocolHeader Automated Tests

> Cap nhat lan cuoi: Phase 1A.2D hoan thanh
> Xem them: [README.md](README.md) | [09_IMPLEMENTATION_STATUS.md](09_IMPLEMENTATION_STATUS.md)

---

## 1. Muc tieu Phase 1A.2D

Phase 1A.2D khong them chuc nang moi vao production code. Muc tieu duy nhat la **kiem chung** rang hai ham da implement o Phase 1A.2B va 1A.2C hoat dong chinh xac:

| Ham can kiem chung | Phase goc | Noi dung |
|---|---|---|
| `serializeHeader()` | 1A.2B | `ProtocolHeader` → 24-byte `QByteArray` (Big Endian) |
| `deserializeHeader()` | 1A.2C | 24-byte `QByteArray` → `std::optional<ProtocolHeader>` |

**Khong co thay doi nao** duoc thuc hien doi voi:
- Production code (`ProtocolSerializer.h / .cpp`, `ProtocolHeader.h`, `ProtocolConstants.h`)
- Networking layer (TCP, socket, buffer)
- Cac Phase tiep theo

Tat ca 6 test cases chi kiem tra **logic serialize/deserialize header** o tang protocol, hoan toan doc lap voi mang.

---

## 2. Test framework

### 2.1 Qt Test

Du an su dung **Qt Test** - framework unit test tich hop san trong Qt SDK. Qt Test khong can cai dat them thu vien ngoai, phu hop voi du an da su dung Qt Core.

```cpp
// ProtocolSerializerTests.cpp - dong 1
#include <QTest>
```

Qt Test cung cap:
- Macro `QCOMPARE(actual, expected)` - so sanh hai gia tri, bao loi neu khac nhau
- Macro `QVERIFY(condition)` - kiem tra dieu kien boolean, bao loi neu false
- Macro `QTEST_APPLESS_MAIN(TestClass)` - tao ham `main()` khong can `QApplication`

### 2.2 QTEST_APPLESS_MAIN

```cpp
// ProtocolSerializerTests.cpp - dong 162
QTEST_APPLESS_MAIN(ProtocolSerializerTests)
```

`QTEST_APPLESS_MAIN` la bien the cua `QTEST_MAIN` danh cho cac test **khong can GUI** va **khong can event loop**. No sinh ra ham `main()` gon nhe, khoi chay tat ca cac slot trong class test, roi thoat voi exit code = so test bi fail.

Su dung `QTEST_APPLESS_MAIN` thay vi `QTEST_MAIN` vi:
- `ProtocolSerializer` chi lam viec voi byte arrays va so nguyen - khong can `QApplication`
- Giam phu thuoc, giam thoi gian khoi dong test executable
- Phu hop voi CI/CD (khong can display/framebuffer)

### 2.3 QCOMPARE va QVERIFY

| Macro | Cach dung | Khi nao dung |
|---|---|---|
| `QCOMPARE(actual, expected)` | So sanh hai gia tri cung kieu | Kiem tra gia tri cu the (field, size, byte array) |
| `QVERIFY(condition)` | Kiem tra dieu kien boolean | Kiem tra boolean (`.has_value()`, `!.has_value()`) |

Khi mot macro that bai, Qt Test in thong bao loi ro rang, vi du:
```
FAIL!  : ProtocolSerializerTests::testExactSerialization()
Compared values are not the same
   Actual   (data)  : "\x52\x44\x54\x50..."
   Expected (expected): "\x52\x44\x54\x51..."
```

### 2.4 Test executable rieng

Test nay duoc build thanh mot executable rieng (`ProtocolSerializerTests`), hoan toan tach biet khoi app chinh (`RemoteAccessApp`). Nhu vay:
- App chinh khong bi anh huong khi chay test
- Test co the chay don doc hoac qua CTest
- CI co the chay test ma khong can launch GUI

### 2.5 CTest / add_test

```cmake
# tests/CMakeLists.txt - dong 12
add_test(NAME ProtocolSerializerTests COMMAND ProtocolSerializerTests)
```

`add_test` dang ky test voi **CTest** - he thong test runner tich hop cua CMake. Sau khi dang ky, co the chay tat ca test bang:

```bash
cd build
ctest --output-on-failure
```

Hoac cu the:
```bash
ctest -R ProtocolSerializerTests -V
```

---

## 3. Giai thich tung test

File test: `clients/RemoteAccessApp/tests/Protocol/ProtocolSerializerTests.cpp`

Tat ca test nam trong class `ProtocolSerializerTests : public QObject`, cac method test la `private slots` theo quy uoc cua Qt Test.

---

### 3.1 `testDefaults()`

**Doan code (dong 14-24):**
```cpp
void testDefaults()
{
    ProtocolHeader header(MessageType::PING);
    QCOMPARE(header.magic, PROTOCOL_MAGIC);
    QCOMPARE(header.version, PROTOCOL_VERSION);
    QCOMPARE(header.type, MessageType::PING);
    QCOMPARE(header.flags, 0);
    QCOMPARE(header.payloadLength, 0);
    QCOMPARE(header.sessionId, 0ULL);
    QCOMPARE(header.sequenceNumber, 0);
}
```

**Muc dich:** Kiem tra constructor `ProtocolHeader(MessageType)` khoi tao dung 7 field.

**7 field can kiem tra:**

| Field | Gia tri mong doi | Kieu |
|---|---|---|
| `magic` | `0x52445450` (`PROTOCOL_MAGIC`) | `uint32_t` |
| `version` | `0x01` (`PROTOCOL_VERSION`) | `uint8_t` |
| `type` | `MessageType::PING` (do caller truyen vao) | `MessageType` |
| `flags` | `0` | `uint16_t` |
| `payloadLength` | `0` | `uint32_t` |
| `sessionId` | `0ULL` | `uint64_t` |
| `sequenceNumber` | `0` | `uint32_t` |

**Ly do test nay quan trong:** Neu constructor khong set dung default values, cac test sau co the cho ket qua sai. Test nay la "baseline" cho tat ca test con lai.

---

### 3.2 `testExactSerialization()`

**Doan code (dong 26-65):**
```cpp
void testExactSerialization()
{
    ProtocolHeader header(MessageType::PING); // type byte = 0x41
    header.flags = 0x1234;
    header.payloadLength = 0x01020304;
    header.sessionId = 0x0102030405060708ULL;
    header.sequenceNumber = 0x11223344;

    QByteArray data = ProtocolSerializer::serializeHeader(header);
    QCOMPARE(data.size(), static_cast<int>(HEADER_SIZE)); // == 24

    QByteArray expected;
    // magic: 0x52445450 big-endian
    expected.append(static_cast<char>(0x52));
    expected.append(static_cast<char>(0x44));
    expected.append(static_cast<char>(0x54));
    expected.append(static_cast<char>(0x50));
    // version: 0x01
    expected.append(static_cast<char>(0x01));
    // type: PING = 0x41
    expected.append(static_cast<char>(0x41));
    // flags: 0x1234 big-endian
    expected.append(static_cast<char>(0x12));
    expected.append(static_cast<char>(0x34));
    // payloadLength: 0x01020304 big-endian
    expected.append(static_cast<char>(0x01));
    expected.append(static_cast<char>(0x02));
    expected.append(static_cast<char>(0x03));
    expected.append(static_cast<char>(0x04));
    // sessionId: 0x0102030405060708 big-endian
    expected.append(static_cast<char>(0x01));
    // ... (8 bytes total)
    expected.append(static_cast<char>(0x08));
    // sequenceNumber: 0x11223344 big-endian
    expected.append(static_cast<char>(0x11));
    expected.append(static_cast<char>(0x22));
    expected.append(static_cast<char>(0x33));
    expected.append(static_cast<char>(0x44));

    QCOMPARE(data, expected);
}
```

**Muc dich:** Kiem tra `serializeHeader()` tao ra dung layout 24 bytes theo RDTP wire format.

**Layout 24 bytes duoc kiem tra:**

| Offset | Bytes | Noi dung | Gia tri trong test |
|---|---|---|---|
| 0-3 | 4 | `magic` (Big Endian) | `52 44 54 50` |
| 4 | 1 | `version` | `01` |
| 5 | 1 | `type` (uint8_t) | `41` (PING) |
| 6-7 | 2 | `flags` (Big Endian) | `12 34` |
| 8-11 | 4 | `payloadLength` (Big Endian) | `01 02 03 04` |
| 12-19 | 8 | `sessionId` (Big Endian) | `01 02 03 04 05 06 07 08` |
| 20-23 | 4 | `sequenceNumber` (Big Endian) | `11 22 33 44` |

**Tai sao so sanh byte-by-byte quan trong?**

So sanh toan bo `QByteArray` voi `QCOMPARE(data, expected)` dam bao:

1. **Kich thuoc chinh xac:** Neu serialize tao 23 hoac 25 bytes, test that bai ngay
2. **Thu tu byte chinh xac:** Big Endian la bat buoc trong network protocol. Neu byte thu tu bi dao nguoc (Little Endian), RDTP se bi broken khi giao tiep giua cac may khac endianness
3. **Khong co byte thua:** Moi byte deu duoc kiem tra, khong co padding an hoac garbage bytes
4. **Gia tri chinh xac o tung offset:** Vi du flag `0x1234` phai xuat hien tai offset 6-7 la `12 34`, khong phai `34 12`

Mot test chi kiem tra `size == 24` la chua du - no khong phat hien duoc endianness sai hoac field nao bi dat sai offset. So sanh byte-by-byte la cach duy nhat de kiem chung **toan bo wire format**.

---

### 3.3 `testDeserialization()`

**Doan code (dong 67-105):**
```cpp
void testDeserialization()
{
    QByteArray data;
    data.append(static_cast<char>(0x52)); // ... 24 bytes (same layout as above)
    // ...

    std::optional<ProtocolHeader> result = ProtocolSerializer::deserializeHeader(data);
    QVERIFY(result.has_value());

    QCOMPARE(result->magic, 0x52445450U);
    QCOMPARE(result->version, 0x01U);
    QCOMPARE(result->type, MessageType::PING);
    QCOMPARE(result->flags, 0x1234U);
    QCOMPARE(result->payloadLength, 0x01020304U);
    QCOMPARE(result->sessionId, 0x0102030405060708ULL);
    QCOMPARE(result->sequenceNumber, 0x11223344U);
}
```

**Muc dich:** Kiem tra `deserializeHeader()` chuyen 24 bytes thanh `ProtocolHeader` dung.

**Quy trinh:**
1. Xay dung thu cong 24 bytes dung wire format (khong qua `serializeHeader`)
2. Goi `deserializeHeader(data)`
3. Kiem tra `result.has_value()` - phai co gia tri (khong phai `nullopt`)
4. Kiem tra chinh xac 7 field

**Tai sao xay dung byte array thu cong thay vi dung output cua `testExactSerialization`?**

Test nay muon chung minh rang `deserializeHeader` hoat dong **doc lap** voi `serializeHeader`. Neu test nay chi nhan input tu `serializeHeader`, no khong biet byte array dau vao co chinh xac khong. Bang cach xay dung byte array thu cong tu raw hex values, ta dam bao:
- Input biet truoc chinh xac (khong phu thuoc vao ham khac)
- Ket qua kiem chung doc lap hoan toan
- Test nay la "ground truth" cho deserializer

---

### 3.4 `testRoundTrip()`

**Doan code (dong 107-126):**
```cpp
void testRoundTrip()
{
    ProtocolHeader original(MessageType::SCREEN_FRAME);
    original.flags = 0x9999;
    original.payloadLength = 123456;
    original.sessionId = 9876543210ULL;
    original.sequenceNumber = 42;

    QByteArray data = ProtocolSerializer::serializeHeader(original);
    std::optional<ProtocolHeader> result = ProtocolSerializer::deserializeHeader(data);

    QVERIFY(result.has_value());
    QCOMPARE(result->magic, original.magic);
    QCOMPARE(result->version, original.version);
    QCOMPARE(result->type, original.type);
    QCOMPARE(result->flags, original.flags);
    QCOMPARE(result->payloadLength, original.payloadLength);
    QCOMPARE(result->sessionId, original.sessionId);
    QCOMPARE(result->sequenceNumber, original.sequenceNumber);
}
```

**Muc dich:** Kiem tra **tinh nhat quan** (consistency) giua serialize va deserialize.

**Luong du lieu:**
```
ProtocolHeader (original)
    → serializeHeader()
    → QByteArray (24 bytes)
    → deserializeHeader()
    → std::optional<ProtocolHeader> (result)
    → QCOMPARE moi field voi original
```

**Diem khac biet voi cac test truoc:**
- Dung `MessageType::SCREEN_FRAME` (0x10) thay vi `PING` (0x41) - test MessageType khac nhau
- Dung `sessionId = 9876543210ULL` - so lon hon 32-bit, kiem tra 64-bit encoding
- Test nay khong biet cu the tung byte la gi, chi quan tam rang output phai giong input

**Round-trip test la dieu kien can thiet nhung chua du:**

Round-trip PASS chi chung minh serialize va deserialize nhat quan voi nhau. Nhung neu ca hai deu sai theo cung mot cach (vi du ca hai deu dung Little Endian), round-trip van PASS. Do do round-trip phai di kem voi `testExactSerialization` va `testDeserialization` de dam bao chinh xac tuyet doi.

---

### 3.5 `testInvalidSizes()`

**Doan code (dong 128-138):**
```cpp
void testInvalidSizes()
{
    QByteArray data0;
    QVERIFY(!ProtocolSerializer::deserializeHeader(data0).has_value());

    QByteArray data23(23, '\0');
    QVERIFY(!ProtocolSerializer::deserializeHeader(data23).has_value());

    QByteArray data25(25, '\0');
    QVERIFY(!ProtocolSerializer::deserializeHeader(data25).has_value());
}
```

**Muc dich:** Kiem tra `deserializeHeader()` xu ly input sai kich thuoc mot cach an toan.

**3 truong hop duoc kiem tra:**

| Input | Kich thuoc | Mong doi |
|---|---|---|
| `data0` | 0 bytes (rong) | `std::nullopt` |
| `data23` | 23 bytes (thieu 1) | `std::nullopt` |
| `data25` | 25 bytes (thua 1) | `std::nullopt` |

**Tai sao kiem tra ca 23 bytes va 25 bytes?**

RDTP header luon la **dung 24 bytes** - khong hon, khong kem. Deserializer phai tu choi bat ky input nao co kich thuoc sai, du chi lech 1 byte. Ly do:

- **23 bytes:** Neu doc duoc, sequenceNumber (offset 20-23) se bi thieu 1 byte, dan den undefined behavior khi doc 4 bytes tu offset 20
- **25 bytes:** Neu doc duoc, deserializer dang bo qua 1 byte cuoi, nghia la no se chap nhan packet bi noi them byte rac hoac bi loi frame
- **0 bytes:** Edge case quan trong, tranh crash khi xu ly connection moi chua nhan du du lieu

**Ket qua phai la `std::nullopt`**, khong phai crash hay exception. Day la API contract cua `deserializeHeader`.

---

### 3.6 `testHighBytes()`

**Doan code (dong 140-159):**
```cpp
void testHighBytes()
{
    ProtocolHeader original(MessageType::ERROR); // type byte = 0x70
    original.flags = 0x80FF;
    original.payloadLength = 0x89ABCDEF;
    original.sessionId = 0xFEDCBA9876543210ULL;
    original.sequenceNumber = 0xFFEEDDCC;

    QByteArray data = ProtocolSerializer::serializeHeader(original);
    std::optional<ProtocolHeader> result = ProtocolSerializer::deserializeHeader(data);

    QVERIFY(result.has_value());
    QCOMPARE(result->magic, original.magic);
    QCOMPARE(result->version, original.version);
    QCOMPARE(result->type, original.type);
    QCOMPARE(result->flags, original.flags);
    QCOMPARE(result->payloadLength, original.payloadLength);
    QCOMPARE(result->sessionId, original.sessionId);
    QCOMPARE(result->sequenceNumber, original.sequenceNumber);
}
```

**Muc dich:** Kiem tra correctness khi cac field co **bit cao (MSB) bang 1** - phat hien loi sign extension.

**Cac gia tri duoc chon co chu y:**

| Field | Gia tri | Ly do chon |
|---|---|---|
| `flags` | `0x80FF` | Bit 15 = 1 (MSB cua uint16_t bi dat) |
| `payloadLength` | `0x89ABCDEF` | Bit 31 = 1 (MSB cua uint32_t bi dat), byte `0x89 > 0x7F` |
| `sessionId` | `0xFEDCBA9876543210ULL` | Bit 63 = 1 (MSB cua uint64_t bi dat), nhieu byte > 0x7F |
| `sequenceNumber` | `0xFFEEDDCC` | Bit 31 = 1, byte `0xFF`, `0xEE`, `0xDD`, `0xCC` deu > 0x7F |

**Loi sign extension la gi?**

`QByteArray` luu tru du lieu duoi dang `char` (co dau trong C++). Khi doc 1 byte tu `QByteArray` ma gia tri byte do lon hon `0x7F` (bit cao bang 1), neu khong cast sang `uint8_t` truoc, C++ se thuc hien **sign extension** - mo rong dau am ra cac byte cao hon:

```cpp
// Sai - sign extension xay ra
char c = data[i];               // c = (char)0x89 = -119 (signed char)
uint32_t value = (uint32_t)c;   // value = 0xFFFFFF89  <-- sai!

// Dung - cast qua uint8_t truoc
uint8_t b = (uint8_t)data[i];   // b = 0x89 = 137
uint32_t value = (uint32_t)b;   // value = 0x00000089  <-- chinh xac
```

**Hau qua cua sign extension:**
- `payloadLength = 0x89ABCDEF` co the bi deserialize thanh `0xFFFFFF89...` hoac gia tri sai khac
- `flags = 0x80FF` se bi doc sai neu byte `0x80` bi sign-extend
- `sessionId = 0xFEDCBA9876543210` se sai o nhieu byte

**Test nay la safety net:** Neu ai sua code va vo tinh dung `char` thay `uint8_t` o mot buoc trung gian trong `deserializeHeader`, `testHighBytes` se that bai ngay, trong khi `testRoundTrip` (dung gia tri nho khong vuot 0x7F) van co the PASS.

---

## 4. CMake test integration

### 4.1 Kich hoat test o top-level CMakeLists.txt (canonical configure root)

**File:** `clients/CMakeLists.txt` *(top-level canonical configure root)*

```cmake
# Enable CTest from this top-level configure root
enable_testing()

# Add subdirectory for the main GUI client
add_subdirectory(RemoteAccessApp)
```

`enable_testing()` phai duoc goi tai **chinh file CMakeLists.txt duoc dung lam configure root** (`-S .`). Neu goi trong sub-directory (`RemoteAccessApp/CMakeLists.txt`), CTest o top build directory se khong nhan biet cac test, va:

```bash
ctest --test-dir build_tests
# -> No tests found
```

Trong khi:
```bash
ctest --test-dir build_tests/RemoteAccessApp
# -> Found ProtocolSerializerTests
```

Sau khi doi `enable_testing()` len `clients/CMakeLists.txt`:
```bash
ctest --test-dir build_tests
# -> 1/1 Test #1: ProtocolSerializerTests ...  Passed
```

### 4.2 Tim kiem Qt Test component

**File:** `clients/RemoteAccessApp/tests/CMakeLists.txt`

```cmake
# Dong 1
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
```

`Qt::Test` la component rieng biet, khong duoc bao gom trong `Qt::Core` hay `Qt::Widgets`. Can goi `find_package` rieng de tim `Qt6::Test` (hoac `Qt5::Test` tuy phien ban). Bien `QT_VERSION_MAJOR` duoc CMake tu dien sau khi `find_package(QT NAMES Qt6 Qt5 ...)` o root `CMakeLists.txt`.

### 4.3 Tao test executable

```cmake
# Dong 3-5
add_executable(ProtocolSerializerTests
    Protocol/ProtocolSerializerTests.cpp
)
```

Tao mot executable rieng chi chua code test. Executable nay:
- **Khong phai** `RemoteAccessApp` chinh
- Co ham `main()` do `QTEST_APPLESS_MAIN` sinh ra tu `.moc` file
- Build rieng biet, co the chay doc lap

### 4.4 Lien ket thu vien

```cmake
# Dong 7-10
target_link_libraries(ProtocolSerializerTests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Network
)
```

Hai thu vien can thiet:

| Thu vien | Ly do |
|---|---|
| `Qt::Test` | Cung cap `QTest`, `QCOMPARE`, `QVERIFY`, `QTEST_APPLESS_MAIN` |
| `Network` | CMake target chua `ProtocolSerializer`, `ProtocolHeader`, `ProtocolConstants` - phai link de truy cap production code can test |

**Khong** link `Qt::Core`, `Qt::Widgets`, hay `Qt::Gui` vi test khong can GUI. (`Qt::Test` da tu link `Qt::Core` khi can.)

### 4.5 Dang ky voi CTest

```cmake
# Dong 12
add_test(NAME ProtocolSerializerTests COMMAND ProtocolSerializerTests)
```

| Tham so | Gia tri | Y nghia |
|---|---|---|
| `NAME` | `ProtocolSerializerTests` | Ten hien thi trong output cua `ctest` |
| `COMMAND` | `ProtocolSerializerTests` | Executable can chay (tim trong build directory) |

Sau khi dang ky, co the chay bang:
```bash
# Chay tat ca test (can Qt DLL trong PATH)
# Windows MinGW:
# set PATH=C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;%PATH%
ctest --test-dir build_tests

# Voi output day du
ctest --test-dir build_tests -V

# Bat output khi co test that bai
ctest --test-dir build_tests --output-on-failure
```

> **Luu y Windows:** CTest chay executable trong working directory cua no, khong ke thua PATH cua shell mot cach tu dong. De tranh loi `0xc0000135` (DLL not found), phai dam bao `C:\Qt\6.11.1\mingw_64\bin` (hoac thu muc Qt DLL tuong ung) co mat trong `PATH` truoc khi chay `ctest`.

---

## 5. Ket qua hien tai

Ket qua sau khi build va chay test Phase 1A.2D:

| Hang muc | Ket qua |
|---|---|
| Build production | PASS |
| Build tests | PASS |
| `testDefaults` | PASS |
| `testExactSerialization` | PASS |
| `testDeserialization` | PASS |
| `testRoundTrip` | PASS |
| `testInvalidSizes` | PASS |
| `testHighBytes` | PASS |
| **Tests passed** | **6** |
| **Tests failed** | **0** |
| App launch (kiem tra regression) | PASS |

```
Test project .../RemoteAccessApp/build
    Start 1: ProtocolSerializerTests
1/1 Test #1: ProtocolSerializerTests .........   Passed

100% tests passed, 0 tests failed out of 1
```

---

## 6. Scope - Nhung gi Phase nay CHUA lam

Phase 1A.2D **chi kiem chung header serialize/deserialize**. Tat ca cac phan duoi day **chua ton tai** trong codebase:

| Phan chua implement | Lien quan den |
|---|---|
| `ProtocolParser` | Doc va phan tich packet tu TCP byte stream |
| TCP buffering | Xu ly partial reads, reassembly tu socket |
| TCP connection | QTcpSocket, QTcpServer, ket noi mang thuc su |
| Relay routing | Logic chuyen tiep packet qua Relay server |
| Session management | Tao, theo doi, va huy session |
| Payload parsing | Phan tich payload cua tung `MessageType` |
| Screen streaming | `SCREEN_FRAME` payload encode/decode |
| Input forwarding | `MOUSE_*`, `KEY_*` payload |
| Telemetry processing | `TELEMETRY` payload |

> **Luu y quan trong:** `MessageType::SCREEN_FRAME` xuat hien trong `testRoundTrip` va `MessageType::ERROR` xuat hien trong `testHighBytes` **chi la gia tri enum de test header field**. Khong co logic xu ly payload tuong ung nao duoc implement. Phase 1A.2D khong them chuc nang networking moi.

---

## 7. Tai lieu lien quan

| Tai lieu | Noi dung |
|---|---|
| [04_PROTOCOL_HEADER_MODEL.md](04_PROTOCOL_HEADER_MODEL.md) | `struct ProtocolHeader`: 7 fields, constructor, default values |
| [05_WIRE_HEADER_FORMAT.md](05_WIRE_HEADER_FORMAT.md) | 24-byte wire layout: offset, kich thuoc, thu tu |
| [06_HEADER_SERIALIZATION.md](06_HEADER_SERIALIZATION.md) | `serializeHeader()`: implementation, Big Endian helpers |
| [07_BYTE_ORDER_BIG_ENDIAN.md](07_BYTE_ORDER_BIG_ENDIAN.md) | Ly thuyet Big Endian / Network Byte Order |
| [08_CMAKE_INTEGRATION.md](08_CMAKE_INTEGRATION.md) | CMake: target `Network`, include paths |
| [09_IMPLEMENTATION_STATUS.md](09_IMPLEMENTATION_STATUS.md) | Trang thai toan bo cac Phase |
| [10_HEADER_DESERIALIZATION.md](10_HEADER_DESERIALIZATION.md) | `deserializeHeader()`: implementation, sign extension |
