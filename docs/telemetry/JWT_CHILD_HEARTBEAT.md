# Heartbeat CHILD sử dụng JWT

## 1. Mục đích

Heartbeat hiện tại dùng để CHILD định kỳ chứng minh:

- Client vẫn đang chạy.
- JWT hợp lệ.
- Role trong JWT là `CHILD`.
- JWT subject tương ứng với một `Child` tồn tại.

**IMPORTANT:**
Heartbeat hiện tại KHÔNG lưu `ONLINE`/`OFFLINE` state vào database.
Heartbeat hiện tại là read-only đối với database.

---

## 2. Kiến trúc và trách nhiệm

### Client

- **AppController**: Quản lý lifecycle. Sau khi login thành công, kiểm tra role và chỉ khởi động `HeartbeatReporter` cho `CHILD`.
- **AuthService**: Thực hiện chức năng login, nhận JWT và lưu JWT thông qua `ApiClient`.
- **ApiClient**: Lưu JWT, quản lý HTTP request và tự động thêm header `Authorization: Bearer <JWT>`.
- **HeartbeatReporter**: Quản lý `QTimer`, thu thập `deviceUid` và `name`. Gửi heartbeat ngay khi `start()` và tiếp tục gửi mỗi 5000 ms bằng cách gọi `ApiClient::post()`. KHÔNG tự xử lý JWT và KHÔNG tự tạo Bearer header.

### Server

- **ChildController**: Cung cấp endpoint HTTP heartbeat. Xử lý `Authorization` header và chuyển raw token cùng request xuống `ChildService`.
- **ChildService**: Kiểm tra heartbeat business rule. Từ chối (reject) token thiếu/sai/hết hạn. Yêu cầu role là `CHILD`. Lấy username từ JWT subject và gọi `ChildRepository`.
- **JwtUtil**: Đảm nhận việc parse JWT, verify signature bằng `parseClaimsJws(...)`, kiểm tra expiration, `extractRole(...)` và `extractUsername(...)`.
- **ChildRepository**: Thực hiện truy vấn `findByChildUsername(jwtUsername)`.

---

## 3. GRASP

- **AppController** → Đảm nhận trách nhiệm lifecycle/controller (Controller pattern).
- **HeartbeatReporter** → Information Expert cho heartbeat timing và thu thập device metadata.
- **ApiClient** → Information Expert / Pure Fabrication cho authenticated HTTP.
- **ChildController** → HTTP Controller.
- **ChildService** → Xử lý business logic.
- **JwtUtil** → Trách nhiệm chuyên biệt về JWT (High Cohesion).
- **ChildRepository** → Xử lý persistence lookup.

**High Cohesion & Low Coupling:**
Hệ thống có tính liên kết cao (High Cohesion) và ghép nối lỏng lẻo (Low Coupling). Đặc biệt, `HeartbeatReporter` không cần biết:
- JWT được lưu ở đâu.
- Cú pháp `Bearer`.
- Cách tạo `Authorization` header.

Tất cả các trách nhiệm liên quan đến bảo mật và HTTP header được giao cho `ApiClient`, giúp các lớp tách biệt rõ ràng.

---

## 4. Luồng đăng nhập JWT

Luồng hiện tại hoạt động như sau:

CHILD
→ login
→ `AuthService`
→ `POST /api/v1/auth/login`
→ Server tạo JWT
→ JWT trả về Client
→ `ApiClient::setToken(...)`
→ role `CHILD`
→ `AppController`
→ `HeartbeatReporter.start()`

**JWT identity:**
- Subject = username
- Role
- Expiration

Hệ thống không ghi JWT thật và không ghi password thật ra log hoặc database ngoại trừ luồng xác thực ban đầu.

---

## 5. Lifecycle heartbeat

**CHILD:**
- Login thành công
- → `AppController::handleLoginSuccess(...)`
- → `QGuiApplication::setQuitOnLastWindowClosed(false)`
- → Tạo đúng một `HeartbeatReporter` (với parent là `AppController`)
- → `start()`
- → Gửi heartbeat đầu tiên ngay lập tức
- → Khởi chạy `QTimer` 5000 ms.

**ADMIN:**
- Mở luồng ADMIN hiện tại.
- KHÔNG tạo `HeartbeatReporter`.
- KHÔNG gửi heartbeat.

---

## 6. HTTP Contract

**Exact endpoint:**
`POST /api/v1/child/heartbeat`

**Headers:**
- `Content-Type: application/json`
- `Authorization: Bearer <JWT>`

**Exact client body:**
```json
{
  "deviceUid": "...",
  "name": "..."
}
```

**Clarification on Identity:**
- `username` **KHÔNG** được sử dụng từ body làm danh tính xác thực (authenticated identity).
- Authenticated identity hoàn toàn đến từ: **JWT subject**.
- Required role: **CHILD**.

*Lưu ý: Nếu `HeartbeatRequest.java` vẫn còn chứa các trường `username` hay `ipAddress`, chúng chỉ là các optional legacy DTO fields và hoàn toàn **KHÔNG** phải là danh tính xác thực. Client hiện tại không gửi các trường này trong payload.*

---

## 7. Server validation flow

`POST /api/v1/child/heartbeat`
→ `ChildController`
→ extract Bearer token
→ `ChildService.handleHeartbeat(...)`
→ reject missing token
→ `JwtUtil` parses signed JWT
→ signature verification
→ expiration check
→ role == `CHILD`
→ extract username from subject
→ `ChildRepository.findByChildUsername(username)`
→ Nếu `Child` tồn tại → success (HTTP 200)
→ Nếu thiếu `Child` → reject (HTTP 401 Unauthorized)

---

## 8. Database behavior

Entity `Child` hiện tại chỉ chứa các trường logic cơ bản:
- `id`
- `childUsername`
- `password`
- `owner` (map tới entity `User` qua `user_id`)

**Heartbeat được thiết kế dưới dạng READ ONLY.**

Heartbeat **KHÔNG** persist hoặc thay đổi các trường sau:
- `status`
- `lastSeen`
- `ipAddress`
- `deviceUid`
- `name`
- `password`
- `owner`
- `childUsername`

Không có bất kỳ lệnh `.save()` nào liên quan đến heartbeat. Lookup duy nhất được thực hiện là:
`ChildRepository.findByChildUsername(jwtUsername)`

**Tính năng JWT heartbeat hiện tại không thay đổi schema database.**

---

## 9. Runtime / failure behavior

- Heartbeat đầu tiên được gửi ngay lập tức (immediate first heartbeat).
- Chu kỳ gửi (interval) là 5000 ms.
- Lỗi mạng không làm terminate CHILD; các tick sau đó có thể retry.
- Missing / invalid JWT sẽ bị reject.
- Expired token sẽ bị reject.
- Sai role (khác `CHILD`) sẽ bị reject.
- Unknown `Child` (không tìm thấy trong database) sẽ bị reject.

---

## 10. Manual tests actually completed

**CHILD**
- Login successful.
- JWT received.
- CHILD heartbeat started.
- HTTP 200 repeatedly approximately every 5 seconds.
- **Result:** PASS

**ADMIN**
- Login successful.
- No `HeartbeatReporter` startup.
- No heartbeat request.
- **Result:** PASS

---

## 11. End-to-End flow

CHILD
↓ login
Management API
↓
JWT
↓
ApiClient lưu JWT
↓
AppController
↓
HeartbeatReporter
↓ mỗi 5 giây
ApiClient
↓ Authorization: Bearer JWT
POST /api/v1/child/heartbeat
↓
ChildController
↓
ChildService
↓
JwtUtil
↓
JWT subject = childUsername
↓
ChildRepository.findByChildUsername(...)
↓
success / reject

---

## 12. Technical debt

Các khoản nợ kỹ thuật hiện tại dựa trên source code đang chạy:
- JWT validation hiện đang thực hiện thủ công (manual) tại Controller/Service thay vì sử dụng Spring Security filter.
- API base URL (ngrok) hiện tại đang bị hardcode.
- Heartbeat không lưu trạng thái online/offline vào database.
- Không có cơ chế tự động phát hiện offline.
- `HeartbeatRequest` có thể vẫn giữ lại một số optional legacy fields (`username`, `ipAddress`) không còn được sử dụng.
