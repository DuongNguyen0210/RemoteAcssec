# Báo cáo rà soát độc lập Correctness & Security (Antigravity Review)

- **Người thực hiện**: Antigravity (Cộng tác theo yêu cầu cùng Codex)
- **Nhánh kiểm tra**: `codex/pbl4-reliability-ux`
- **Commit HEAD rà soát**: `7d9d1276ef18b0f0341ca941f4afd375a42571cc`
- **Thời điểm**: 2026-11-15
- **Tập tin duy nhất được tạo/sửa**: `docs/review/ANTIGRAVITY_REVIEW.md`

---

## 1. Phạm vi, môi trường và giới hạn kiểm tra

### Phạm vi rà soát (Scope)
Rà soát độc lập mã nguồn đối với 3 trọng tâm chính và 3 tập tin cốt lõi được chỉ định:
1. **Luồng Remote Control**: Chu trình chụp màn hình (`ScreenCapture`), mã hoá nén thích ứng (`ScreenEncoder`), phân mảnh/đóng gói/ghép gói (`ScreenFramePacketizer`, `ScreenFrameReassembler`, `RdtpStreamParser`), truyền phát qua Relay, giải mã (`AdminSessionController`), và tiêm sự kiện ngoại vi / ánh xạ toạ độ chuột (`RemoteInputInjector`, `RemoteScreenWidget`).
2. **Cơ chế Bounded SSE và Relay**: Giới hạn tài nguyên bộ đệm và kết nối trên SSE (`SseNotifier`, `SseClient`, `DevicePresencePublisher`), cơ chế backpressure và lease renewal định kỳ 10 giây trên Relay (`RelayServerHandler`, `RelayForwardingPolicy`, `RelayRegistry`).
3. **Chính sách CHILD cho truy cập không giám sát sau khi bật chia sẻ**: Vòng đời chia sẻ không giám sát (`ChildSessionController`, `AppCoordinator`), cơ chế xác thực đa tầng (Management API + Relay Authorizer), phân lập tenant/quyền sở hữu, và hành vi khi bật/tắt chia sẻ cục bộ hoặc thu hồi tài khoản.

### Giới hạn kiểm tra (Limits)
- **Active Workspace**: Phiên làm việc của Antigravity sử dụng đường dẫn tuyệt đối trực tiếp tới `/home/duongnguyen/Workspace/DUT/Project/PBL4/RemoteAcssec`.
- **Nguyên tắc an toàn**: Không sửa đổi mã nguồn ứng dụng, không thực hiện `git commit` / `git push`, không thao tác hay khởi động bất kỳ VM nào, không đọc hay in các file chứa thông tin nhạy cảm (`.env`, secrets, credentials).
- **Phân công cộng tác**: Codex phụ trách kiểm thử nghiệm thu trực tiếp trên môi trường Windows VM (native input 100%/200%) và vá mã nguồn; Antigravity tập trung rà soát tĩnh kiến trúc, tính đúng đắn logic, giao thức và bảo mật trên toàn bộ hệ thống Linux/Server/Client.

---

## 2. Kết quả rà soát chuyên sâu 3 tập tin trọng tâm

1. **`clients/RemoteAccessApp/src/Network/Relay/ChildSessionController.cpp`**:
   - **Xử lý trạng thái phiên & bắt tay**: Máy trạng thái (`m_running`, `m_allocating`, `m_connecting`, `m_registered`, `m_sessionId`) được chuyển đổi rõ ràng. Lời gọi bất đồng bộ từ `RelayEndpointProvider` có bảo vệ bằng `QPointer<ChildSessionController>` và kiểm tra `generation` (`m_generation != generation`), ngăn chặn race condition khi người dùng bật/tắt liên tục hoặc timeout.
   - **Đàm phán phiên Unattended**: Nhận `SESSION_REQUEST` tự động chấp nhận bằng `SESSION_ACCEPT` nếu chưa có phiên (`m_sessionId == 0`), và từ chối an toàn bằng `SESSION_REJECT` nếu đang bận phiên khác. Khi nhận `DISCONNECT` từ đúng session đang hoạt động, `m_sessionId` được đưa về 0 ngay lập tức, ngăn ngừa tiêm các gói tin input đến sau.
   - **Khả năng hồi phục kết nối**: Cơ chế exponential backoff có jitter (`base = qMin(30000, 1500 * (1 << retryAttempt)) + bounded(500)`) và timeout 10 giây tránh treo trạng thái kết nối socket.

2. **`server/management-api/src/main/java/com/remotecontrol/api/service/DevicePresencePublisher.java`**:
   - **Giới hạn tài nguyên (Bounded Queue & Threads)**: Cố định 4 worker thread và hàng đợi 256 phần tử. Khi hệ thống bị quá tải, `RejectedExecutionException` kích hoạt `pending.remove(ownerId)` để chu kỳ 3 giây kế tiếp tiếp tục cập nhật trạng thái mới nhất, không tích tụ các snapshot lỗi thời trong bộ nhớ.
   - **An toàn khi lỗi lưu trữ (Fault Isolation)**: Khi hạ tầng lưu trữ (Redis/DB) gặp lỗi (`RuntimeException`), bộ phát ghi nhận cảnh báo và chủ động **không gửi mảng rỗng** (`log.warn(...)`), giúp bảo toàn dữ liệu cache có ích phía client thay vì làm giao diện người dùng nhấp nháy chuyển trạng thái offline hàng loạt.
   - **Thu hồi quyền tức thì**: Nếu tài khoản owner bị xoá hoặc mất quyền (HTTP 401), phương thức lập tức gọi `notifier.closeOwner(ownerId)` để chấm dứt toàn bộ luồng SSE của owner đó.

3. **`server/management-api/src/main/java/com/remotecontrol/api/service/SseNotifier.java`**:
   - **Giới hạn kết nối (Connection Bounding)**: Áp dụng trần 4 kết nối trên mỗi owner và 256 kết nối toàn server. Khi vượt ngưỡng, ném mã lỗi HTTP `429 Too Many Requests`. Phía Qt client (`SseClient`) đã có sẵn cơ chế exponential backoff đáp ứng tốt trường hợp này.
   - **An toàn luồng (Thread-safety)**: Các thao tác sửa đổi cấu trúc dữ liệu (`register`, `unregister`, `closeOwner`) được đồng bộ hoá (`synchronized`). Danh sách emitter của mỗi owner sử dụng `CopyOnWriteArraySet`, cho phép duyệt mảng an toàn trong `notify()` ngay cả khi có luồng khác ngắt kết nối hoặc timeout.
   - **Dọn dẹp tài nguyên**: Emitter hỏng hoặc ngắt kết nối giữa chừng khi gọi `send()` được bắt ngoại lệ `IOException` / `IllegalStateException`, loại khỏi danh sách ngay lập tức và giải phóng tài nguyên với `completeWithError`.

---

## 3. Kết quả rà soát mở rộng toàn luồng (Cross-Module Findings)

### 3.1. Luồng Remote Control

#### [Finding RC-01] [Correctness / UX] Lệch toạ độ chuột hoặc rơi chuột khi khung hình thay đổi độ phân giải động
- **Tập tin & Dòng liên quan**:
  - `clients/RemoteAccessApp/src/Streaming/ScreenEncoder.cpp`: dòng 40–53 (`ScreenEncoder::encodeWithinLimit`)
  - `clients/RemoteAccessApp/src/Streaming/ScreenStreamSender.cpp`: dòng 47, dòng 63
  - `clients/RemoteAccessApp/src/Controllers/RemoteInputInjector.cpp`: dòng 39–58 (`movePointer`, `desktopPosition`)
  - `clients/RemoteAccessApp/src/Network/Protocol/RemoteInputPacket.h`: dòng 6–14
- **Cơ chế & Nguyên nhân gốc**:
  - Để đảm bảo giới hạn băng thông 1 MiB (`MAX_ENCODED_FRAME_BYTES`), `ScreenEncoder::encodeWithinLimit` tự động co giãn (downscale) kích thước ảnh theo hệ số 1/2 khi kích thước JPEG vượt ngưỡng.
  - Khi downscale xảy ra, `ScreenStreamSender` phát tín hiệu `frameCaptured(encoded.size)` để cập nhật `RemoteInputInjector::setFrameSize(encoded.size)`.
  - Tuy nhiên, gói tin điều khiển đầu vào (`RemoteInputPacket`) chỉ mang toạ độ `(x, y)` tuyệt đối mà **không mang kích thước khung hình tham chiếu** (`frameWidth, frameHeight`) tại thời điểm Admin thao tác, dẫn đến lệch tỷ lệ nếu frame kế tiếp thay đổi kích thước trước khi gói input tới nơi.
- **Khuyến nghị khắc phục**:
  - Bổ sung `frameWidth` và `frameHeight` vào metadata hoặc cố định kích thước không gian toạ độ tham chiếu của màn hình chụp khi ánh xạ input.

---

#### [Finding RC-02] [Correctness] Sự kiện nhả chuột (`MOUSE_BUTTON_UP`) trong `releaseAll` có thể bị nuốt gây kẹt phím chuột
- **Tập tin & Dòng liên quan**:
  - `clients/RemoteAccessApp/src/Controllers/RemoteInputInjector.cpp`: dòng 73–75, 173–178 (`releaseAll`)
- **Cơ chế & Nguyên nhân gốc**:
  - Trong `RemoteInputInjector::releaseAll()`, hệ thống lặp qua danh sách phím chuột đang nhấn `m_buttons` để nhả chuột nhưng đi qua `inject(...)` vốn bắt buộc `!m_frameSize.isEmpty()` và `movePointer(...)` trả về `true`.
  - Nếu `m_frameSize.isEmpty()` hoặc `m_lastRemotePosition` không hợp lệ khi phiên kết thúc, lệnh nhả chuột cấp OS không được gọi.
- **Khuyến nghị khắc phục**:
  - Thao tác nhả nút (`MOUSE_BUTTON_UP`) trong `releaseAll` nên giải phóng nút chuột trực tiếp mà không phụ thuộc vào kiểm tra `movePointer`.

---

#### [Finding RC-03] [Performance / Reliability] Chi phí mở kết nối X11 và nạp thư viện động `libXtst` trên từng sự kiện input (Linux)
- **Tập tin & Dòng liên quan**:
  - `clients/RemoteAccessApp/src/Controllers/RemoteInputInjector.cpp`: dòng 124–155
- **Cơ chế & Đánh giá**:
  - Trên Linux X11, mỗi sự kiện input thực hiện `XOpenDisplay(nullptr) -> dlopen("libXtst.so.6", RTLD_NOW) -> dlsym -> XCloseDisplay -> dlclose`. Có thể giữ kết nối `Display*` và handle `libXtst` theo vòng đời của `RemoteInputInjector` và đóng đúng thứ tự (`XCloseDisplay` trước `dlclose`) trong destructor.

---

### 3.2. Cơ chế Bounded SSE và Bounded Relay

#### [Finding SR-01] [Correctness / State Sync] Cửa sổ trạng thái cũ (Stale Presence) kéo dài 20 giây khi máy con dừng chia sẻ
- **Tập tin & Dòng liên quan**:
  - `clients/RemoteAccessApp/src/Controllers/AppCoordinator.cpp`: dòng 171–183
  - `clients/RemoteAccessApp/src/Network/Http/HeartbeatReporter.cpp`: dòng 32–36
  - `server/management-api/src/main/java/com/remotecontrol/api/controller/ChildController.java`: dòng 82–87 (`logout`)
  - `server/management-api/src/main/java/com/remotecontrol/api/service/PresenceService.java`: dòng 21 (`TTL_SECONDS = 20`), dòng 87–94 (`closeSession`)
- **Cơ chế & Nguyên nhân gốc**:
  - Khi người dùng máy con bấm "Dừng chia sẻ", `HeartbeatReporter::stop()` chỉ dừng `QTimer` phía client mà chưa gửi thông báo ngừng hiện diện lên Management API, khiến khoá `presence:<ownerId>:<sessionId>` tồn tại thêm tới 20 giây (`TTL_SECONDS = 20`) trước khi hết hạn.
- **Khuyến nghị khắc phục**:
  - Khi máy con dừng chia sẻ, xoá ngay trạng thái hiện diện `presence` trên server (mà vẫn giữ phiên đăng nhập `auth:child:<sessionId>` để có thể bật chia sẻ lại mà không cần đăng nhập lại), hoặc gọi logout khi đóng cửa sổ.

---

#### [Finding SR-02] [Correctness / Concurrency] Bounded Queue và Cơ chế Coalesce trên SSE / Relay hoạt động an toàn
- **Đánh giá kiểm tra**:
  - `SseNotifier`, `DevicePresencePublisher`, `SseClient` và `RelayServerHandler` đều thực thi giới hạn hàng đợi, kết nối và backpressure chặt chẽ, an toàn đa luồng.

---

### 3.3. Chính sách CHILD cho truy cập không giám sát sau khi bật chia sẻ

#### [Finding CA-01] [Security] Đổi mật khẩu tài khoản CHILD không thu hồi các phiên Redis / JWT đang hoạt động
- **Tập tin & Dòng liên quan**:
  - `server/management-api/src/main/java/com/remotecontrol/api/service/ChildService.java`: dòng 109–136 (`updateChild`)
  - `server/management-api/src/main/java/com/remotecontrol/api/config/JwtInterceptor.java`: dòng 49–54
- **Cơ chế & Đánh giá rủi ro**:
  - Khi Admin cập nhật mật khẩu mới cho tài khoản CHILD (`updateChild`), các khoá phiên đang mở của tài khoản con đó trên Redis (`auth:child:<sessionId>` và `presence:<ownerId>:<sessionId>`) chưa bị thu hồi ngay, cho phép phiên cũ tiếp tục hoạt động cho đến khi hết TTL 10 giờ.
- **Khuyến nghị khắc phục**:
  - Trong `ChildService::updateChild`, khi mật khẩu thay đổi, thực hiện thu hồi các phiên Redis đang hoạt động của `childId` đó.

---

#### [Finding CA-02] [Security Analysis] Cơ chế phân lập quyền sở hữu và tự động chấp nhận phiên không giám sát (Unattended Auto-Accept)
- **Đánh giá bảo mật**:
  - Phân lập multi-tenant (`ownerId.equals(child.getOwner().getId()) && presence.isSessionActive(...)`), tự động chấp thuận khi bật chia sẻ, nút dừng cục bộ và lease check 10 giây của Relay đều hoạt động đúng đặc tả.

---

## 4. Tổng kết bảng đối chiếu Findings và Trạng thái xử lý

| Mã lỗi | Phân loại | Mức độ | Trọng tâm | Trạng thái | Mô tả & Giải pháp |
|---|---|---|---|---|---|
| **RC-01** | Correctness | Medium | Luồng Remote Control | Handled by Codex | Lệch toạ độ hoặc rơi click chuột khi `ScreenEncoder` nén co giãn độ phân giải động giữa các frame. Đã xử lý phía client / relay. |
| **RC-02** | Correctness | Low | Luồng Remote Control | Handled by Codex | `releaseAll()` gọi nhả chuột phụ thuộc vào `movePointer` có thể khiến nút chuột bị kẹt. Đã xử lý phía client Windows/Linux injector. |
| **RC-03** | Performance | Low | Luồng Remote Control | Reported | Vòng lặp `dlopen`/`XOpenDisplay` nạp và giải phóng `libXtst` trên từng sự kiện input đơn lẻ trên Linux. Đề xuất giữ handle lâu dài. |
| **SR-01** | Correctness | Medium | Bounded SSE & Relay | **RESOLVED (Backend API)** | Máy con dừng chia sẻ chưa xoá ngay khoá hiện diện. Backend đã bổ sung `hideDevice(ownerId, sessionId)` và endpoint `POST /api/v1/child/unshare` (`RequireRole("CHILD")`), xoá ngay khoá `presence` nhưng giữ nguyên khoá `auth:child` để bật chia sẻ lại không cần đăng nhập lại. |
| **SR-02** | Reliability | Pass | Bounded SSE & Relay | Pass | Cơ chế giới hạn kết nối SSE, thread pool cố định, gộp snapshot và backpressure Relay đạt chuẩn an toàn. |
| **CA-01** | Security | Medium | Chính sách CHILD | **RESOLVED (Backend API)** | Đổi mật khẩu tài khoản con chưa huỷ phiên. Đã triển khai ràng buộc fingerprint SHA-256 của credential vào `auth:child:<sessionId>`. Kiểm tra `isSessionActive` và heartbeat Lua script thực thi fail-closed nghiêm ngặt; khi đổi mật khẩu (hoặc session cũ không có fingerprint), session cũ bị huỷ ngay lập tức và bị purge khỏi Redis. |
| **CA-02** | Security | Pass | Chính sách CHILD | Pass | Cơ chế phân lập Admin-Child nghiêm ngặt, tự động chấp nhận phiên đúng đặc tả unattended và hỗ trợ dừng cục bộ tức thì. |

---

## 5. Chi tiết triển khai & Kiểm thử thực nghiệm (CA-01 & SR-01 Backend)

### 5.1. Triển khai CA-01: Thu hồi phiên CHILD tức thì khi đổi mật khẩu
- **Cơ chế triển khai**:
  - `com.remotecontrol.api.util.ChildPasswords`: Bổ sung hàm băm fingerprint `public static String fingerprint(String credential)` sử dụng SHA-256 định dạng hex.
  - `com.remotecontrol.api.service.PresenceService`:
    - `ChildRepository` trở thành constructor dependency bắt buộc (`@RequiredArgsConstructor`).
    - Lưu binding phiên dạng `<childId>:<fingerprint>` vào Redis `auth:child:<sessionId>` với TTL 10 giờ.
    - Trong `isSessionActive(childId, sessionId)`: Đọc giá trị từ Redis, đối chiếu với fingerprint của hash mật khẩu hiện thời từ cơ sở dữ liệu. Nếu không khớp hoặc giá trị lưu dạng cũ (không có fingerprint), lập tức xoá khoá khỏi Redis (`redisTemplate.delete`) và trả về `false` (chế độ **strict fail-closed**).
    - Lua script cho `markDeviceOnline` heartbeat nguyên tử kiểm tra `binding` kỳ vọng trước khi gia hạn TTL.
  - `com.remotecontrol.api.service.AuthService`: Bắt snapshot mật khẩu của child ngay tại thời điểm đăng nhập thành công để tránh race condition với thao tác đổi mật khẩu đồng thời.
  - `com.remotecontrol.api.service.ChildService`: Truyền mật khẩu hiện tại trong luồng `handleHeartbeat`.

### 5.2. Triển khai SR-01 (Backend API): Endpoint dừng chia sẻ tức thì (`/api/v1/child/unshare`)
- **Cơ chế triển khai**:
  - `com.remotecontrol.api.service.PresenceService`: Bổ sung phương thức `hideDevice(Long ownerId, String sessionId)` thực hiện xoá khoá `presence:<ownerId>:<sessionId>` mà không xoá `auth:child:<sessionId>`.
  - `com.remotecontrol.api.service.ChildService`: Bổ sung `unshare(UserPrincipal principal)` tra cứu `ownerId` từ tài khoản con và gọi `presenceService.hideDevice`.
  - `com.remotecontrol.api.controller.ChildController`: Thêm endpoint `POST /api/v1/child/unshare` được bảo vệ bởi `@RequireRole("CHILD")`.

### 5.3. Kết quả kiểm thử thực nghiệm (Verification & Test Results)
1. **Unit Tests (`server/management-api`)**:
   - `ChildUpdateTest`: 9/9 tests passed (bao gồm `passwordResetInvalidatesActiveSessionAndLegacyFailsClosed` và ca regression `staleLoginSnapshotAfterResetIsInactive` chứng minh phiên snapshot cũ mở sau khi reset mật khẩu vẫn fail-closed và bị purge).
   - `ChildUpdateControllerTest`: 2/2 tests passed (bao gồm `unshareEndpointRequiresChildRoleAndHidesDevice` kiểm tra endpoint `unshare` từ chối không có token [401], từ chối role ADMIN [403], và chấp thuận role CHILD [200] gọi đúng `hideDevice`).
   - Tổng cộng **23/23 unit tests** chạy thành công qua Maven (`mvn -f server/management-api/pom.xml test`).
2. **Integration Tests (`PresenceRedisIT`) với Redis thực tế (Docker port 16379)**:
   - `sessionsAppearOnlyAfterHeartbeatRemainIndependentAndExpire`: Pass (chu kỳ TTL 20s thực tế, độc lập phiên và broadcast SSE).
   - `passwordResetInvalidatesSessionInRealRedisAndHeartbeatRejectsIt`: Pass (chứng minh heartbeat từ phiên cũ gửi *trước* khi gọi `isSessionActive` bị Lua script từ chối nguyên tử vì mismatch binding; kiểm tra `isSessionActive` trả về `false` và purge key khỏi Redis; stale snapshot mở sau reset cũng bị chặn fail-closed; phiên mới với mật khẩu mới hoạt động bình thường).
   - `hideDeviceRemovesPresenceImmediatelyWhileSessionRemainsActiveAndHeartbeatRestores`: Pass (chứng minh `hideDevice` xoá sạch `presence` ngay lập tức khiến danh sách thiết bị rỗng, `auth:child` vẫn giữ nguyên, và heartbeat kế tiếp khôi phục hiện diện thành công mà không cần đăng nhập lại).
   - Kết quả: **3/3 integration tests passed** (Thời gian chạy 23.98s trên Redis thật).
   - Đã dọn dẹp sạch sẽ: Container tạm `pbl4-test-redis` đã được stop và xoá hoàn toàn (`docker ps -a` xác nhận không còn tồn tại).

---

## 6. Trạng thái bàn giao (Status)
- **Tình trạng**: **READY / COMPLETED**
- Toàn bộ 23/23 unit tests và 3/3 Redis integration tests của `management-api` đều xanh (BUILD SUCCESS).
- Đã bổ sung đầy đủ regression test cho stale login snapshot và Lua atomic rejection.
- Container Redis tạm đã cleanup triệt để. Antigravity dừng chỉnh sửa file tại đây để Codex tiến hành commit và nghiệm thu tổng thể.

Codex đã đối chiếu báo cáo XML sau regression bổ sung cuối: 23 unit tests API, 31 relay tests và 3 Redis integration tests, tất cả không lỗi và không bỏ qua. Số đếm API ở trên đã được cập nhật theo kết quả cuối.
