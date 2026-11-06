# Implementation Plan: Phase 6 - SSE-based Device Presence Push Notification

## Overview

Hiện tại client Admin phải **fetch thủ công** (`GET /api/v1/devices`) mỗi khi muốn thấy danh sách thiết bị online. Tính năng này thêm cơ chế **Server-Sent Events (SSE)** để server tự động push thông báo khi trạng thái thiết bị thay đổi (online/offline), giúp UI Admin cập nhật realtime mà không tốn băng thông polling.

### Tại sao SSE thay vì WebSocket hay Long-Polling?

| Cơ chế        | Độ phức tạp | Hướng dữ liệu | Phù hợp với use-case |
|---------------|-------------|---------------|----------------------|
| Polling       | Thấp        | Pull          | Hiện tại - tốn tài nguyên |
| Long-Polling  | Trung bình  | Pull-ish      | Phức tạp server state |
| **SSE**       | **Thấp**    | **Server→Client** | ✅ Phù hợp nhất - đơn giản, HTTP/1.1 |
| WebSocket     | Cao         | Bidirectional | Over-engineering - không cần 2 chiều ở đây |

### Thiết kế không quá tải server

- **1 SSE connection per ADMIN user** (không phải per-device)
- **Không có dedicated thread** - dùng Spring MVC async (SseEmitter)
- **In-process notify**: heartbeat/logout handler → `SseNotifier.notify(ownerId)` → push snapshot mới
- **Timeout** SSE connection sau 60 giây, client tự reconnect
- **Client-side**: QNetworkAccessManager + QNetworkReply với manual HTTP streaming parse

---

## Architecture Decisions

1. **SSE endpoint**: `GET /api/v1/devices/events` trả `text/event-stream`, auth bằng JWT Bearer header
2. **Event types**: chỉ 1 event type `device-list` gửi toàn bộ danh sách mới nhất (snapshot)
3. **Trigger**: khi `PresenceService.markDeviceOnline()` hoặc `closeSession()` → `SseNotifier.notify(ownerId)` → push snapshot
4. **Thread-safety**: `ConcurrentHashMap<Long, CopyOnWriteArraySet<SseEmitter>>` lưu active emitters
5. **Qt client**: `DeviceSseStream` mở SSE stream, parse từng `data: {...}` → emit signal → store cập nhật → UI tự refresh

---

## Task List

### Phase 1: Server - SSE Infrastructure
- [ ] Task 1: Implement `SseNotifier` service
- [ ] Task 2: Implement SSE endpoint trong `DeviceController`
- [ ] Task 3: Tích hợp trigger vào `PresenceService` + `WebConfig`

### Checkpoint Server
- [ ] Server build OK
- [ ] curl test SSE endpoint trả stream

### Phase 2: Client - SSE Consumer
- [ ] Task 4: Implement `DeviceSseStream` Qt
- [ ] Task 5: Tích hợp `DeviceSseStream` vào `DeviceService` + `AppCoordinator`

### Checkpoint E2E
- [ ] Admin UI tự cập nhật khi child heartbeat

### Phase 3: Polish
- [ ] Task 6: Docs + commit

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Spring MVC thread bị giữ bởi SSE lâu | Medium | SseEmitter async + timeout 60s |
| Qt không support EventSource natively | Medium | QNetworkReply stream read tự implement |
| Nhiều admin cùng login | Low | CopyOnWriteArraySet per ownerId |
| SSE connection bị drop | Low | Client reconnect tự động sau 3s |
