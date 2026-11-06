# Kế hoạch triển khai: Phase 5 - ADMIN Tiếp nhận, Ghép khung hình & Hiển thị màn hình từ xa (Screen Reassembly, Decoding & View)

## 1. Tổng quan (Overview)
Sau khi hoàn thành Phase 4, hệ thống đã hỗ trợ Relay chuyển tiếp các chunk `SCREEN_FRAME` (RDTP type `0x10`) gắn với `sessionId` của phiên `ACTIVE` tới socket TCP của ADMIN.
Giai đoạn này tập trung vào việc hoàn thiện phía **ADMIN Client**:
1. Tách và xây dựng độc lập module **`ScreenFrameReassembler`** trong C++ tuân thủ Clean Architecture và Single Responsibility Principle (thay thế cho code chắp vá đang để tạm trong `AdminSessionController`).
2. Tích hợp giải mã JPEG (`QImage`) độc lập và luồng quản lý khung hình.
3. Xây dựng giao diện hiển thị **`RemoteDesktopWindow`** (GUI) chuyên nghiệp, hỗ trợ scale tỉ lệ khung hình (keep aspect ratio), thanh công cụ điều khiển và nút Ngắt kết nối (Disconnect).
4. Kết nối toàn bộ luồng từ `AdminSessionController` qua `DevicesController` đến `AppCoordinator` và `RemoteDesktopWindow`.
5. Bổ sung Unit test cho `ScreenFrameReassembler` để đảm bảo độ tin cậy và không bị hồi quy.

---

## 2. Kiến trúc & Nguyên tắc thiết kế (Architecture & Design Principles)

### Các nguyên tắc áp dụng:
- **Single Responsibility Principle (SRP):**
  - `AdminSessionController`: Chỉ lo việc quản lý trạng thái phiên RDTP qua Relay. Khi có `SCREEN_FRAME`, chuyển tiếp payload thô cho `ScreenFrameReassembler` xử lý.
  - `ScreenFrameReassembler`: Chuyên trách việc nhận chunk, ghép chunk theo `frameId`, quản lý hàng đợi in-flight frames, phát hiện frame hoàn chỉnh hoặc frame bị lỗi/stale.
  - `RemoteDesktopWindow`: Chỉ phụ trách hiển thị hình ảnh (`QImage`), vẽ trên màn hình và tiếp nhận tương tác UI từ người dùng (nút Đóng/Ngắt kết nối).
- **High Cohesion & Low Coupling:**
  - `RemoteDesktopWindow` không bị gắn chặt với giao thức mạng RDTP hay `RelayClient`. Giao diện chỉ nhận dữ liệu frame (hoặc `QImage`) thông qua Signals/Slots hoặc Controller.
- **Tái sử dụng & Dễ mở rộng (Reusability & Extensibility):**
  - Wire format của `SCREEN_FRAME` (Big Endian: `frameId`, `chunkIndex`, `chunkCount`, `totalFrameSize`) được đóng gói chuẩn, dùng chung hằng số từ `ScreenFramePacketizer::SCREEN_FRAME_METADATA_SIZE`.

---

## 3. Danh sách tác vụ (Task Breakdown)

### Task 1: Xây dựng `ScreenFrameReassembler` (Domain / Streaming Layer)
- Tạo lớp `ScreenFrameReassembler` độc lập trong `clients/RemoteAccessApp/src/Streaming/` (hoặc `Network/Protocol/`).
- Hỗ trợ:
  - Phân tích Big Endian metadata (`frameId`, `chunkIndex`, `chunkCount`, `totalFrameSize`).
  - Hàng đợi `m_frames` giới hạn (`MAX_IN_FLIGHT_FRAMES = 4`), tự động loại bỏ frame cũ nhất khi đầy để chống tràn bộ nhớ.
  - Ghép đầy đủ dữ liệu JPEG và trả về `QByteArray` hoàn chỉnh khi nhận đủ các chunk.
  - Bỏ qua các chunk trùng lặp (duplicate).
- Tách khỏi `AdminSessionController`, xóa bỏ struct lộn xộn `FrameAssembly` trong `AdminSessionController`.

### Task 2: Unit Test cho `ScreenFrameReassembler`
- Tạo test case (hoặc test executable) kiểm tra:
  - Ghép 1 frame đơn lẻ gồm 1 chunk.
  - Ghép 1 frame gồm nhiều chunk (đúng thứ tự và đảo thứ tự).
  - Xử lý payload bị lỗi (kích thước < 16 bytes, chunkIndex >= chunkCount).
  - Cơ chế eviction khi số lượng frame in-flight vượt quá giới hạn.

### Task 3: Tích hợp `ScreenFrameReassembler` vào `AdminSessionController` & Phát tín hiệu Frame
- Trong `AdminSessionController`:
  - Sở hữu một instance `ScreenFrameReassembler`.
  - Khi nhận `MessageType::SCREEN_FRAME` thuộc session hiện tại: chuyển payload vào `ScreenFrameReassembler`.
  - Khi một frame JPEG hoàn tất, phát tín hiệu `frameReceived(quint32 frameId, const QByteArray &jpegData)`.
  - Dọn dẹp cache `ScreenFrameReassembler` khi kết nối Relay bị ngắt hoặc phiên kết thúc.

### Task 4: Xây dựng Giao diện `RemoteDesktopWindow` (GUI)
- Tạo `RemoteDesktopWindow` trong `clients/RemoteAccessApp/src/GUI/Windows/`:
  - Widget hiển thị màn hình: render `QImage` giữ đúng tỉ lệ gốc (`Qt::KeepAspectRatio`).
  - Header bar: Hiển thị tên thiết bị điều khiển, sessionId, trạng thái kết nối, nút "Ngắt kết nối" (Disconnect).
  - Tín hiệu `disconnectRequested()`.

### Task 5: Tích hợp vào `DevicesController` và `AppCoordinator`
- `DevicesController`:
  - Kết nối tín hiệu `frameReceived` từ `AdminSessionController` và forward lên hoặc quản lý phiên hiển thị.
  - Lắng nghe yêu cầu disconnect để gửi tín hiệu hủy phiên / đóng socket.
- `AppCoordinator`:
  - Khi `remoteSessionStarted(sessionId, agentSessionId)` được kích hoạt: tạo và mở `RemoteDesktopWindow`.
  - Kết nối luồng frame vào `RemoteDesktopWindow`.
  - Khi cửa sổ đóng hoặc người dùng bấm "Ngắt kết nối": thực hiện đóng phiên và dọn dẹp tài nguyên.

### Task 6: Kiểm tra biên dịch, chạy thử nghiệm và hoàn thiện tài liệu
- Biên dịch toàn bộ client C++ qua CMake.
- Cập nhật tài liệu trạng thái triển khai trong `docs/protocol/09_IMPLEMENTATION_STATUS.md`.
- Tạo các git commit theo atomic commit convention.
