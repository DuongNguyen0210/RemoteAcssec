# Review theo tên đề tài — 2026-11-13

**Đề tài:** Xây dựng hệ thống điều khiển máy tính từ xa phân tán ứng dụng kiến trúc Relay Proxy và Load Balancing.

> Đây là review lịch sử. Các mục TLS, chính sách unattended và script E2E bên
> dưới đã được xử lý sau review: relay hỗ trợ TLS có kiểm tra hostname; CHILD
> phải bật chia sẻ và có nút dừng; script trong `server/tests/` đã được quản lý
> bằng Git. Xem `PBL4_IMPROVEMENT.md` và `COMPLETION_AUDIT.md` cho trạng thái
> hiện tại, số liệu kiểm thử và các phần nghiệm thu hai máy còn thiếu.

## Trạng thái Git và phạm vi

- Đã chạy `git fetch origin --prune`; `origin/main` là `5939507`.
- Nhánh hiện tại: `codex/pbl4-reliability-ux`. Tại thời điểm merge/review,
  HEAD là `b70ba7d`; hai mục Qt P1 sau đó được commit tại `664917f`.
- `git rev-list --left-right --count HEAD...origin/main`: `9 0`.
- `git merge --no-edit origin/main`: `Already up to date.`; main là tổ tiên
  của HEAD, không có xung đột hoặc merge commit mới cần tạo.
- Review cả mã đã commit và thay đổi đang có trong working tree. Những thay
  đổi cấu hình/SSE/JWT của phiên làm việc vẫn chưa commit; chưa push remote.

## Đánh giá

Review ban đầu chỉ ra hai khoảng trống trong core. Hai mục P1 đầu tiên bên dưới
đã được triển khai sau review trong working tree: ADMIN lookup relay theo CHILD;
viewer gửi input và CHILD inject trên Windows/X11. Cần thử trên hai máy thật.

| Thành phần | Đã có | Còn thiếu |
|---|---|---|
| Màn hình từ xa | Capture/encode JPEG, packetize, relay, reassemble, viewer | Kiểm chứng trên hai máy thật và số liệu chất lượng |
| Điều khiển từ xa | ADMIN bắt chuột/phím, gửi RDTP qua relay; CHILD inject bằng X11/XTest hoặc Windows SendInput | Kiểm chứng tương tác thật; Wayland chưa hỗ trợ |
| Relay Proxy | Registry phiên, xác thực/ownership, ngắt phiên, timeout, backpressure | TLS transport và chính sách quyền điều khiển trên CHILD |
| Load Balancing | Redis Lua cấp slot theo tỷ lệ sử dụng, affinity, heartbeat/lease; Nginx hai API; ADMIN lookup endpoint từng CHILD | Đo hiệu quả khi có tải thực |
| Phân tán | Nhiều process API/relay, PostgreSQL/Redis dùng chung | Demo nhiều host, kịch bản node chết/phục hồi, quan sát tải từng node |

## Phát hiện theo ưu tiên

### P1 — ADMIN kết nối tới relay được cấp cho CHILD: đã xử lý trong Qt

`AdminSessionController` giờ dùng `RelayEndpointProvider::lookup(agentSessionId)`
trước mỗi phiên. Khi `REMOTE_RELAY_MODE=dynamic`, provider hỏi Management API;
khi endpoint đổi, ADMIN đổi kết nối TCP và gửi `CONNECT_REQUEST` tới relay mới.
Callback lookup cũ được chặn bằng generation và `QPointer`. Test dùng hai fake
relay ở hai cổng, kiểm tra phiên thứ hai gửi CONNECT_REQUEST và input đúng node.

### P1 — Điều khiển chuột và bàn phím: đã xử lý cho Windows/X11

`RemoteScreenWidget` bắt move/click/wheel/key, đổi tọa độ viewer sang frame và
`AdminSessionController::sendInput()` gửi RDTP theo session hiện tại. CHILD nối
`inputReceived` với `RemoteInputInjector`, kiểm tra payload và inject bằng
`SendInput` (Windows) hoặc X11/XTest (Linux xcb). Phím/nút đang giữ được thả
khi mất focus hoặc kết thúc phiên. Test tự động kiểm tra mã hóa và đường gửi
qua fake relay; cần chạy thử thao tác thật trên hai máy, nhất là Windows và
màn hình có DPI khác nhau. Wayland chưa có backend inject.

### P1 trước triển khai Internet — Relay gửi token và nội dung qua TCP không TLS

`RelayClient.cpp:5` tạo `QTcpSocket`; `RelayAuthPayload.h:16` chép token vào
payload. `RelayServer.java:39` không có SSL handler trong pipeline. HTTPS của
Management API không mã hóa kết nối TCP trực tiếp tới relay.

**Cần làm:** TLS cho kết nối client–relay, kiểm tra certificate, cung cấp cấu
hình chứng chỉ. Khi demo trong mạng tin cậy có thể nêu rõ giới hạn; không mô tả
phiên relay hiện tại là đã được mã hóa.

### P2 — Phục hồi phiên mới chỉ hoàn chỉnh một phần

CHILD tự cấp lại endpoint và reconnect (`ChildSessionController.cpp:52,176`),
backend có lease 30 giây và reservation 45 giây (`RelayDispatchService.java:19`).
ADMIN kết thúc phiên khi mất transport (`AdminSessionController.cpp:257`) và
chưa lookup lại node. State phiên nằm trong process relay nên không có chuyển
phiên đang chạy sang node khác mà giữ nguyên trạng thái.

**Cần làm:** sau khi sửa lookup, cho ADMIN kết nối lại có giới hạn; test tắt node
đang phục vụ, CHILD chuyển node và ADMIN mở lại phiên. Đo thời gian phát hiện
và phục hồi. Với đồ án, chấp nhận ngắt rồi tạo phiên mới nếu mô tả rõ; live
session migration không bắt buộc chỉ vì tên đề tài có chữ “phân tán”.

### P2 — Cần bằng chứng định lượng cho Load Balancing

`allocate.lua:26` sử dụng `max(reservations, registeredChildren)/capacity`.
Đây là cơ chế cân bằng có thật, nhưng `activeSessions` được heartbeat báo về
(`RelayNodeReporter.java:40`) chưa tham gia lựa chọn. Các CHILD online nhưng
không stream và CHILD stream độ phân giải cao đang được tính gần như nhau.

**Cần làm:** trước tiên giữ thuật toán hiện có và đo phân bố phiên, byte/s,
CPU/RAM từng relay, p50/p95 thời gian tạo phiên, FPS và thời gian phục hồi.
So sánh một relay với hai relay ở cùng mức tải; thêm trường hợp capacity khác
nhau, node đầy và node chết. Chỉ đổi công thức theo active sessions/bandwidth
khi kết quả đo cho thấy cần thiết.

### P2 — Thiếu quyết định rõ về quyền cho phép điều khiển

`ChildSessionController.cpp:120` tự gửi `SESSION_ACCEPT` cho phiên hợp lệ khi
đang rảnh. Đây là unattended access, chưa có hộp thoại đồng ý của người dùng
CHILD hay nút dừng tại máy CHILD.

**Cần làm:** chọn và ghi rõ chính sách. Nếu điều khiển có người giám sát, thêm
accept/reject và nút dừng tại CHILD. Nếu unattended, cấu hình bật rõ ràng và
thể hiện trạng thái phiên đang hoạt động trước khi bổ sung input injection.

## Phạm vi nghiệm thu đề xuất

1. Hai máy CHILD ở hai relay; ADMIN tự lookup và xem đúng từng máy.
2. ADMIN điều khiển chuột/phím trên máy CHILD thật, có quyền cho phép/ngắt.
3. Chạy relay trên ít nhất hai host/VM để chứng minh endpoint không phụ thuộc localhost.
4. Tắt một relay, quan sát hết lease, CHILD chuyển node và mở lại phiên.
5. Báo cáo số liệu một relay/hai relay và phân bố tải, kèm giới hạn hạ tầng.

Nginx, PostgreSQL và Redis hiện vẫn là các thành phần đơn trong Compose; cần
ghi đây là giới hạn availability. Không cần thêm Kubernetes, AI, chat, truyền
file hoặc clipboard để hoàn thành core trước các mục ở trên.

## Kiểm chứng trong lượt review

- Maven: 18 test Management API và 26 test relay, không failure/error.
- Qt build thành công; 7/7 CTest executables pass.
- Không chạy lại Redis IT hoặc bài multi-host/failover trong lượt này.
- Test Qt bao phủ lookup hai relay, gói input trên đúng phiên và sự kiện viewer;
  chưa kiểm chứng input injection trên desktop thật.
- `server/tests/` đang bị `.gitignore` loại khỏi Git, nên script kiểm thử hệ thống
  tồn tại cục bộ không tự có trên checkout mới. Cần đưa bài E2E cần thiết vào
  thư mục được version control trước khi dùng làm tiêu chí nghiệm thu.
