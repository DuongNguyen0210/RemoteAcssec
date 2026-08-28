# Tài liệu Triển khai và Kiểm thử Tính năng Heartbeat (Child)

## Mục đích tài liệu
Tài liệu này trình bày chi tiết về những gì đã được triển khai cho tính năng heartbeat của ứng dụng Client (dành cho CHILD), lý do thực hiện, kiến trúc hiện tại và luồng dữ liệu chính xác từ Client → Server → Database.
Bên cạnh đó, tài liệu giải thích rõ sự khác biệt trong hành vi giữa tài khoản ADMIN và CHILD, danh sách các file đã thay đổi, cấu hình môi trường phát triển cục bộ (local development), các lỗi đã gặp phải trong quá trình triển khai/kiểm thử cùng cách khắc phục, quy trình kiểm thử tích hợp thủ công, kết quả thực tế, những hạn chế đã biết và định hướng phát triển trong tương lai.

## Tổng quan tính năng (Feature Overview)
Tính năng này tập trung vào việc báo cáo trạng thái trực tuyến (online) cơ bản của thiết bị.

**Hành vi dự kiến:**
- **ADMIN:** Khi đăng nhập bằng tài khoản ADMIN -> mở `MainWindow` -> **không** gửi device heartbeat.
- **CHILD:** Khi đăng nhập bằng tài khoản CHILD -> `LoginWindow` đóng lại -> tiến trình (process) ứng dụng vẫn tiếp tục chạy ngầm -> `HeartbeatReporter` khởi động -> gửi heartbeat ngay lập tức -> lặp lại mỗi 5 giây.

**Luồng dữ liệu Heartbeat:**
Qt CHILD Client → HTTP POST → Management API → `DeviceController` → `DeviceService` → `ChildRepository` → PostgreSQL.

**Lưu ý quan trọng:**
Đây là tính năng **BASIC DEVICE HEARTBEAT / ONLINE REPORTING**.
Nó **CHƯA** bao gồm:
- Telemetry CPU
- Telemetry RAM
- Telemetry GPU
- Telemetry băng thông (bandwidth)
- Protocol RDTP
- Relay routing
- Truyền phát màn hình (screen streaming)
- Điều khiển từ xa (remote input)

## Triển khai trên Client
**Các file liên quan:** `HeartbeatReporter.h` / `HeartbeatReporter.cpp`

**Trách nhiệm chính:**
- Quản lý `QTimer` và `QNetworkAccessManager`.
- Lưu trữ các thông tin định danh: `username`, `hostname`/`deviceName`, `deviceUid`.
- Thực hiện gửi định kỳ HTTP POST request.
- Xử lý các lỗi mạng (network error handling).

**Hoạt động của QTimer:**
- Interval = 5000 ms.
- Khi gọi `start()` -> gửi heartbeat đầu tiên ngay lập tức -> bắt đầu timer -> tự động gửi heartbeat mỗi 5 giây.

**Vòng đời QObject (QObject lifetime):**
`HeartbeatReporter` được khởi tạo với parent là `qApp`. Điều này rất quan trọng vì `LoginWindow` có thể bị đóng lại sau khi đăng nhập thành công, nhưng `HeartbeatReporter` phải được duy trì sống để tiếp tục chạy ngầm.

**Thiết lập chạy ngầm:**
Ứng dụng sử dụng `QApplication::setQuitOnLastWindowClosed(false)` để đảm bảo tiến trình CHILD vẫn tiếp tục chạy sau khi tắt cửa sổ cuối cùng.

**Định danh thiết bị (Device identity):**
- `username`: Là `child_username` đã được xác thực thành công.
- `deviceName`: Lấy từ `QHostInfo::localHostName()`.
- `deviceUid`: Được mã hóa Hex từ `QSysInfo::machineUniqueId().toHex()`.
- `fallback`: Nếu `machineUniqueId` rỗng, sẽ sử dụng `hostname` thay thế.
- `ipAddress`: Client cố tình bỏ qua (omitted); server sẽ tự động trích xuất IP từ địa chỉ nguồn của HTTP request.

## Triển khai trên Server
**Endpoint:** `POST /api/v1/devices/heartbeat`

**Các trường trong `HeartbeatRequest`:**
- `deviceUid`
- `name`
- `username`
- `ipAddress` (optional)

**Ngữ nghĩa hiện tại:**
`HeartbeatRequest.username` tương ứng với trường `child_username` của entity `Child`.

**Luồng xử lý tại `DeviceService`:**
- Gọi `findByChildUsername(username)`
- **Không tìm thấy (not found):** -> trả về `false` -> controller trả về HTTP 404 DEVICE_NOT_FOUND -> **KHÔNG** tạo tài khoản User ảo (dummy User).
- **Tìm thấy (found):** -> cập nhật `name` -> cập nhật `ipAddress` -> set `status = ONLINE` -> cập nhật `lastSeen = now` -> lưu lại -> HTTP 200 OK.

**Lưu ý:** `deviceUid` hiện đang bắt buộc (required) theo hợp đồng (request contract) để validation, tuy nhiên entity `Child` chưa có cột `device_uid`. Do đó, `deviceUid` **chưa được lưu vào cơ sở dữ liệu** và không được dùng làm khóa tra cứu.

## Thay đổi quan trọng so với thiết kế cũ
**Hành vi cũ của `DeviceService`:**
heartbeat -> tìm `User` bằng username -> nếu thiếu, tự động tạo `User` mới với `dummy_password` -> kết quả không được sử dụng -> không cập nhật trạng thái của `Child`.

**Hành vi mới:**
heartbeat -> tìm `Child` hiện có -> cập nhật chính xác trạng thái thực tế của thiết bị -> **không bao giờ** tạo fake User.

**Lý do thay đổi:** Việc tạo fake User phá vỡ cấu trúc quan hệ owner (`user_id` NOT NULL) và không phản ánh đúng trạng thái trực tuyến của thiết bị Child. Thay đổi này là bắt buộc để hệ thống theo dõi đúng thiết bị.

## Cấu hình Môi trường Phát triển (Local Development)
Hệ thống hiện tại được cấu hình và kiểm thử thành công trên môi trường cục bộ:
- **Management API:** `http://localhost:9090`
- **Login endpoint:** `http://localhost:9090/api/v1/auth/login`
- **Heartbeat endpoint:** `http://localhost:9090/api/v1/devices/heartbeat`
- **PostgreSQL:** `localhost:5432`
- **Redis:** `localhost:6379`

**QUAN TRỌNG:**
Cấu hình `localhost` được mô tả ở đây chỉ là **CURRENT LOCAL DEVELOPMENT CONFIGURATION** (Cấu hình phát triển cục bộ hiện tại). Nó **KHÔNG PHẢI** là thiết kế triển khai đa máy chủ (multi-machine) cuối cùng.
`localhost` hoạt động tốt khi Qt Client và Java server cùng chạy trên một máy tính. Nếu triển khai trên 2 máy khác nhau, cấu hình `localhost` trên máy CHILD sẽ trỏ về chính máy CHILD đó, gây lỗi kết nối.
Việc tạo một lớp `ApiConfig` dùng chung và cấu hình linh hoạt địa chỉ server sẽ là phần công việc tương lai.
Ngoài ra, `Authservice` ban đầu sử dụng URL ngrok, hiện đã được chuyển sang `localhost` để vượt qua bài test tích hợp nội bộ này.

## Các vấn đề Môi trường Phát triển đã gặp phải (Troubleshooting)

### 8.1 Vấn đề JDK 26 / Lombok
**Dấu hiệu nhận biết:**
- `java.lang.ExceptionInInitializerError`
- `com.sun.tools.javac.code.TypeTag :: UNKNOWN`
- Cảnh báo Lombok yêu cầu bật annotation processing.

**Nguyên nhân:** Project được thiết kế cho Java 17, nhưng IntelliJ mặc định sử dụng JDK 26, gây xung đột với Lombok. (Đây là lỗi tương thích dependency, không phải do phiên bản JDK mới kém chất lượng).

**Cách khắc phục:**
- Cài đặt và sử dụng JDK 17.
- Cấu hình Project SDK = JDK 17.
- Cấu hình Maven Runner JRE = Project JDK 17.
- Bật tính năng annotation processing trong IDE.

### 8.2 Lỗi bytecode/source level vẫn ở Java 8
**Dấu hiệu nhận biết:**
Sau khi chuyển sang JDK 17, xuất hiện lỗi:
- `cannot find symbol method isEmpty()`
- `cannot find symbol method isBlank()`

**Nguyên nhân:** `Optional.isEmpty()` và `String.isBlank()` là các API mới hơn Java 8, nhưng cấu hình bytecode/source của một số module trong IntelliJ vẫn ở mức 1.8. Cụ thể: Project bytecode = 17 nhưng `management-api = 1.8`, `relay-proxy = 1.8`, `server-root = 1.5`.

**Cách khắc phục:**
- Chuyển Java target/language level của project và các module lên 17. (Code không bị lùi lại cú pháp Java 8).

### 8.3 PostgreSQL connection refused
**Dấu hiệu nhận biết:**
`Connection to localhost:5432 refused`, ứng dụng thoát với code 1.

**Nguyên nhân:** Ứng dụng Java Spring Boot biên dịch thành công nhưng container PostgreSQL chưa hoạt động.

**Cách khắc phục:**
- Cài đặt/khởi động Docker Desktop.
- Chạy lệnh: `docker compose up -d` rồi kiểm tra bằng `docker compose ps`.
- Các container mong đợi: `remote_control_db` (PostgreSQL 5432) và `remote_control_redis` (Redis 6379).

### 8.4 Vấn đề Docker Desktop / WSL
**Dấu hiệu nhận biết:**
Thông báo lỗi từ Docker Desktop: "WSL not installed" và "WSL1 is not supported with your current machine configuration. Please enable the Windows Subsystem for Linux optional component."

**Cách khắc phục:**
- Chạy Command Prompt dưới quyền Administrator: `wsl --install`
- Nếu các tính năng tùy chọn của Windows chưa bật:
  ```cmd
  dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart
  dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart
  ```
- Khởi động lại Windows. Cài đặt Ubuntu qua giao diện CLI nếu WSL yêu cầu.
- Xác nhận Docker Desktop báo "Engine running".

### 8.5 Database PostgreSQL trống
**Dấu hiệu nhận biết:**
Server khởi động thành công, Hibernate tự tạo bảng nhưng các bảng chứa 0 dòng.

**Dữ liệu kiểm thử (TEST DATA) đã sử dụng:**
(Đây CHỈ LÀ DỮ LIỆU TEST LOCAL, mật khẩu lưu dạng plaintext do đây là dự án nguyên mẫu - MVP):
- **ADMIN:** username = `parent01`, password = `pass123`
- **CHILD:** child_username = `child_pc01`, password = `pass123`, name = `Test-PC`, status = `OFFLINE`

**Câu lệnh SQL khởi tạo:**
```sql
INSERT INTO users (username, password, max_children)
VALUES ('parent01', 'pass123', 5);

INSERT INTO child (child_username, password, name, status, user_id)
VALUES (
    'child_pc01',
    'pass123',
    'Test-PC',
    'OFFLINE',
    (SELECT id FROM users WHERE username = 'parent01')
);
```

### 8.6 Lỗi kết nối khi Qt Login
**Dấu hiệu nhận biết:**
Cửa sổ Login Qt giữ nguyên trạng thái mở / Báo lỗi "Connect Error". Console có thể in ra lỗi SSL/kết nối. (Dù endpoint local đã test pass bằng `Invoke-RestMethod` trả về `success = True, role = CHILD`).

**Nguyên nhân:** File `Authservice.cpp` vẫn trỏ về URL ngrok cũ.

**Cách khắc phục (tạm thời cho local test):**
- Đổi API URL trong `Authservice.h` thành: `http://localhost:9090/api/v1/auth/login`.

### 8.7 Nút Stop của IntelliJ không giải phóng port 9090
**Dấu hiệu nhận biết:**
Khi test kịch bản Server sập (failure-recovery), client báo heartbeat vẫn HTTP 200 dù người dùng nghĩ server đã stop.

**Cách khắc phục:**
Sử dụng các lệnh PowerShell/CMD để xác định:
- `Test-NetConnection localhost -Port 9090`
- `netstat -ano | findstr :9090`
- Xác định PID của process java.exe: `tasklist /FI "PID eq <PID>"`
- Terminate process: `taskkill /PID <PID> /F`
- Kiểm tra lại: `TcpTestSucceeded : False`.

## Quy trình Kiểm thử Tích hợp Thủ công (Manual Integration Tests)

### TEST A — ADMIN
- **Thao tác:** Đăng nhập `parent01` / `pass123`
- **Kết quả quan sát:** `MainWindow` mở ra. Không có output của `HeartbeatReporter`. ADMIN không gửi heartbeat.
- **Đánh giá:** PASS

### TEST B — CHILD NORMAL HEARTBEAT
- **Thao tác:** Đăng nhập `child_pc01` / `pass123`
- **Kết quả quan sát:** `LoginWindow` đóng lại. Process tiếp tục chạy. Heartbeat bắt đầu ngay lập tức. HTTP 200 lặp lại mỗi ~5 giây.
- **Log mẫu:** `[HeartbeatReporter] Heartbeat OK (HTTP 200) user="child_pc01"`
- **Đánh giá:** PASS

### TEST C — DATABASE UPDATE
- **Thao tác:** Kiểm tra database PostgreSQL sau TEST B.
- **Kết quả quan sát:** Cột `status` = ONLINE, `last_seen` cập nhật liên tục, `name` được cập nhật, `ip_address` được lấy.
- **Đánh giá:** PASS

### TEST D — SERVER FAILURE
- **Thao tác:** Giữ CHILD đang chạy, terminate Management API. Kiểm tra `Test-NetConnection localhost -Port 9090` trả về `False`.
- **Kết quả quan sát:** Client báo lỗi mạng: `Network error: "Connection refused" – will retry on next tick.` Process CHILD vẫn chạy ổn định.
- **Đánh giá:** PASS

### TEST E — SERVER RECOVERY
- **Thao tác:** Khởi động lại `ManagementApiApplication`. KHÔNG khởi động lại hay relogin trên Client Qt.
- **Kết quả quan sát:** Client tự động phục hồi kết nối từ `Connection refused` về trạng thái `Heartbeat OK (HTTP 200)`.
- **Đánh giá:** PASS

## Kết quả Xác minh Hiện tại

| Tính năng | Trạng thái | Chứng cứ / Giải trình |
| :--- | :--- | :--- |
| ADMIN login | PASS | Chuyển vào MainWindow thành công |
| ADMIN no heartbeat | PASS | Không khởi tạo HeartbeatReporter |
| CHILD login | PASS | Xác thực chính xác role CHILD |
| CHILD background process | PASS | Cửa sổ đóng nhưng timer vẫn chạy |
| Immediate heartbeat | PASS | Request đầu tiên gửi lúc start() |
| 5-second heartbeat | PASS | Chu kỳ QTimer ổn định |
| HTTP 200 | PASS | Server phản hồi thành công |
| PostgreSQL update | PASS | Trạng thái ONLINE được ghi nhận |
| Network failure survival | PASS | Báo lỗi nhưng không crash ứng dụng |
| Automatic recovery | PASS | Trở lại OK khi Server khởi động lại |
| Build compile | PASS | Không lỗi cú pháp (C++ / Java) |
| Bionic read-only review | PASS | Rà soát mã tĩnh đạt yêu cầu |

## Những hạn chế đã biết / Nợ kỹ thuật (Known Limitations / Technical Debt)

1. API URLs bị lặp lại ở 2 nơi: `Authservice` và `HeartbeatReporter`.
2. Cấu hình `localhost` hiện tại chỉ hoạt động cho cùng một máy tính.
3. Lớp `ApiConfig` chung/cấu hình địa chỉ server động chưa được triển khai.
4. Trường `deviceUid` không được lưu trữ trong entity `Child`.
5. Server đang định danh thiết bị bằng `child_username` thay vì Unique ID.
6. Server chưa có cơ chế tự động chuyển sang OFFLINE khi mất tín hiệu heartbeat.
7. Pipeline Redis cho heartbeat/telemetry chưa được ứng dụng.
8. Chưa thu thập Telemetry CPU/RAM/GPU.
9. Chưa có giao diện System Tray UX cho CHILD.
10. Chưa cấu hình Windows Service / Autostart.
11. Mật khẩu vẫn đang ở dạng plaintext trong MVP.
12. Endpoint Heartbeat chưa yêu cầu xác thực bảo mật JWT/Token.

## Các Giai đoạn Tương lai (Future Subphases)

Đề xuất thứ tự ưu tiên triển khai sau này (không làm trong phase này):
1. **Shared configurable ApiConfig:** Hỗ trợ local, LAN (2 máy) và Production (tunnel/domain).
2. **CHILD System Tray UX:** Dùng `QSystemTrayIcon`, hiển thị trạng thái online, tùy chọn Exit.
3. **OFFLINE detection:** Worker trên Server kiểm tra timeout của `lastSeen`.
4. **Persistent deviceUid:** Thêm schema và khóa nhận diện chuẩn xác.
5. **Real telemetry metrics:** Bắt đầu thu thập CPU/RAM/GPU.

## Danh sách Files liên đới (Files Involved)

**C++:**
- `HeartbeatReporter.h`
- `HeartbeatReporter.cpp`
- `loginwindow.h`
- `loginwindow.cpp`
- `main.cpp`
- `Network/CMakeLists.txt`
- `Authservice.h` (Thay đổi URL API local)

**Java:**
- `DeviceController.java`
- `DeviceService.java`

**Tài liệu (Docs):**
- `CLIENT_HEARTBEAT_INTEGRATION.md`
- `CHILD_HEARTBEAT_IMPLEMENTATION_AND_TESTING.md`

## Kết luận

Toàn bộ luồng dữ liệu của tính năng CHILD heartbeat cơ bản đã được xác minh hoạt động liền mạch từ đầu tới cuối (end-to-end):
**Qt CHILD → HTTP → Spring Boot → PostgreSQL**

Bên cạnh đó, ứng dụng Client đủ khả năng chống chịu khi bị gián đoạn mạng hoặc Server sập (Management API interruption/recovery) và tự phục hồi không cần can thiệp.
*Lưu ý xác nhận lần cuối: Giao diện System Tray UX và Cấu hình Server đa máy (multi-machine) KHÔNG thuộc phạm vi của điểm kiểm tra (checkpoint) hoàn thành này.*
