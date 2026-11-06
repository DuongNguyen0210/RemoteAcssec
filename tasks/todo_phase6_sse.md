# Todo: Phase 6 - SSE Device Push Notification

## Task 1: Implement `SseNotifier` service (Server)
**Mô tả:** Service lưu trữ `SseEmitter` của các ADMIN đang kết nối, nhận notify khi có thay đổi presence và push snapshot danh sách thiết bị.

**Tiêu chí chấp nhận:**
- [ ] `register(Long ownerId, SseEmitter emitter)` lưu emitter vào map
- [ ] `unregister(Long ownerId, SseEmitter emitter)` gỡ khi complete/timeout/error
- [ ] `notify(Long ownerId, List<DeviceDto> devices)` gửi SSE event `device-list` tới tất cả emitter của owner đó
- [ ] Thread-safe với `ConcurrentHashMap<Long, CopyOnWriteArraySet<SseEmitter>>`

**Files:**
- `server/management-api/src/main/java/com/remotecontrol/api/service/SseNotifier.java`

**Estimated scope:** S

---

## Task 2: Implement SSE endpoint `GET /api/v1/devices/events` (Server)
**Mô tả:** Thêm endpoint vào `DeviceController` trả `text/event-stream`, đăng ký emitter vào `SseNotifier`, gửi snapshot ban đầu ngay khi kết nối.

**Tiêu chí chấp nhận:**
- [ ] Endpoint `GET /api/v1/devices/events` trả `MediaType.TEXT_EVENT_STREAM_VALUE`
- [ ] Auth ADMIN bắt buộc (annotation `@RequireRole("ADMIN")`)
- [ ] Gửi initial snapshot ngay khi client kết nối
- [ ] `SseEmitter` timeout 60s, cleanup sau complete/timeout/error

**Files:**
- `server/management-api/src/main/java/com/remotecontrol/api/controller/DeviceController.java`

**Estimated scope:** S

---

## Task 3: Tích hợp trigger vào `PresenceService` + `WebConfig` (Server)
**Mô tả:** Sau khi `markDeviceOnline()` hoặc `closeSession()` thành công, gọi `SseNotifier.notify(ownerId, snapshot)` để push tới Admin. Cập nhật `WebConfig` để include path events.

**Tiêu chí chấp nhận:**
- [ ] `markDeviceOnline()` gọi `sseNotifier.notify()` sau khi Redis update thành công
- [ ] `closeSession()` gọi `sseNotifier.notify()` sau khi xóa key Redis
- [ ] `WebConfig` intercept path `/api/v1/devices/events`

**Files:**
- `server/management-api/src/main/java/com/remotecontrol/api/service/PresenceService.java`
- `server/management-api/src/main/java/com/remotecontrol/api/config/WebConfig.java`

**Estimated scope:** S

---

## ✅ Checkpoint Server
- [ ] `mvn compile` thành công trong `server/management-api`
- [ ] `curl -H "Authorization: Bearer <token>" http://localhost:9090/api/v1/devices/events` trả stream

---

## Task 4: Implement `DeviceSseStream` Qt (Client)
**Mô tả:** Class Qt mở HTTP GET SSE stream, đọc dữ liệu chunked, parse từng `data: ...` line, emit signal `devicesReceived(QList<DeviceInfo>)`.

**Tiêu chí chấp nhận:**
- [ ] `DeviceSseStream` kế thừa `QObject`
- [ ] `start()` gửi GET request với `Accept: text/event-stream` header + JWT
- [ ] Parse đúng format SSE: `event: device-list\ndata: [...]\n\n`
- [ ] Emit `devicesReceived(QList<DeviceInfo>)` khi parse xong 1 event
- [ ] Tự động reconnect sau 3s nếu stream bị đóng
- [ ] `stop()` hủy reply, không reconnect

**Files:**
- `clients/RemoteAccessApp/src/Network/Http/DeviceSseStream.h`
- `clients/RemoteAccessApp/src/Network/Http/DeviceSseStream.cpp`
- `clients/RemoteAccessApp/src/Network/Http/CMakeLists.txt` (thêm source)

**Estimated scope:** M

---

## Task 5: Tích hợp `DeviceSseStream` vào `DeviceService` + `AppCoordinator` (Client)
**Mô tả:** `DeviceService` sở hữu `DeviceSseStream`, start stream sau login ADMIN, forwarding signal tới `DevicesController` → `DeviceStore` → UI. Vẫn giữ `fetchDevices()` để initial load và manual refresh.

**Tiêu chí chấp nhận:**
- [ ] `DeviceService` có `startSseStream()` và `stopSseStream()`
- [ ] `AppCoordinator` gọi `startSseStream()` sau `handleLoginSuccess("ADMIN")`
- [ ] Signal từ SSE và từ `fetchDevices()` đều dùng chung `devicesResult` signal
- [ ] Khi logout/session end, `stopSseStream()` được gọi

**Files:**
- `clients/RemoteAccessApp/src/Network/Http/DeviceService.h`
- `clients/RemoteAccessApp/src/Network/Http/DeviceService.cpp`
- `clients/RemoteAccessApp/src/Controllers/AppCoordinator.cpp`

**Estimated scope:** M

---

## ✅ Checkpoint End-to-End
- [ ] Client build thành công
- [ ] Child gửi heartbeat → Admin UI tự cập nhật trong vòng 5-6 giây
- [ ] Child logout → Admin UI tự gỡ device khỏi danh sách

---

## Task 6: Tài liệu và commit
**Mô tả:** Cập nhật docs, tạo commit rõ ràng.

**Tiêu chí chấp nhận:**
- [ ] `docs/` hoặc README được cập nhật với flow SSE
- [ ] Git commit message rõ ràng

**Files:**
- `tasks/plan_phase6_sse.md`
- `docs/` (nếu có)

**Estimated scope:** XS
