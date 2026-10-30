# Danh sách tác vụ: Phase 5 - Screen Frame Reassembly, Decoding & View

## Task 1: Thiết kế và cài đặt `ScreenFrameReassembler`
- **Mô tả:** Xây dựng class `ScreenFrameReassembler` độc lập trong tầng Streaming/Network, chịu trách nhiệm tiếp nhận từng chunk payload RDTP `SCREEN_FRAME`, kiểm tra metadata Big Endian, tập hợp chunks của từng `frameId`, quản lý dung lượng in-flight frames để chống tràn RAM, và hoàn trả `QByteArray` JPEG hoàn chỉnh.
- **Tiêu chí chấp nhận:**
  - [x] Tách riêng thành `ScreenFrameReassembler.h` và `ScreenFrameReassembler.cpp`.
  - [x] Hỗ trợ hàm `feed(const QByteArray &payload) -> std::optional<ReassembledFrame>` không throw, an toàn trước payload rác hoặc trùng chunk.
  - [x] Giới hạn tối đa `MAX_IN_FLIGHT_FRAMES` (4 frame), tự động evict frame cũ nhất khi chạm ngưỡng.
  - [x] Trả về mảng byte JPEG hoàn chỉnh khi tất cả chunks của 1 frame đã được nhận đủ.
- **Tệp liên quan:**
  - `clients/RemoteAccessApp/src/Network/Protocol/ScreenFrameReassembler.h`
  - `clients/RemoteAccessApp/src/Network/Protocol/ScreenFrameReassembler.cpp`
- **Trạng thái:** [x] Hoàn thành

## Task 2: Unit test cho `ScreenFrameReassembler`
- **Mô tả:** Viết unit test tự động (tương tự `ProtocolContractTest`) kiểm tra mọi trường hợp biên của bộ ghép frame.
- **Tiêu chí chấp nhận:**
  - [x] Test ghép thành công frame 1 chunk.
  - [x] Test ghép thành công frame nhiều chunks (cả đúng thứ tự và đảo thứ tự).
  - [x] Test xử lý an toàn chunk duplicate, metadata không hợp lệ (size nhỏ hơn header, index >= count, totalSize <= 0).
  - [x] Test eviction khi có quá 4 frame đồng thời chưa hoàn thành.
  - [x] Toàn bộ test PASS với `ctest`.
- **Tệp liên quan:**
  - `clients/RemoteAccessApp/tests/ScreenFrameReassemblerTest.cpp`
  - `clients/RemoteAccessApp/CMakeLists.txt`
- **Trạng thái:** [x] Hoàn thành

## Task 3: Refactor `AdminSessionController` sử dụng `ScreenFrameReassembler`
- **Mô tả:** Loại bỏ struct tạm `FrameAssembly` trong `AdminSessionController`, tích hợp `ScreenFrameReassembler` và phát signal `frameReceived(quint32 frameId, const QByteArray &jpegData)`.
- **Tiêu chí chấp nhận:**
  - [x] Xóa bỏ struct `FrameAssembly` và hash map tạm thời trong `AdminSessionController`.
  - [x] Chuyển tiếp payload `SCREEN_FRAME` của session hiện tại vào `ScreenFrameReassembler`.
  - [x] Phát signal khi frame ghép xong.
  - [x] Reset/dọn dẹp cache của reassembler khi ngắt kết nối hoặc kết thúc phiên.
- **Tệp liên quan:**
  - `clients/RemoteAccessApp/src/Network/Relay/AdminSessionController.h`
  - `clients/RemoteAccessApp/src/Network/Relay/AdminSessionController.cpp`
- **Trạng thái:** [x] Hoàn thành

## Task 4: Xây dựng Giao diện `RemoteDesktopWindow` & Widget hiển thị
- **Mô tả:** Tạo cửa sổ hiển thị màn hình từ xa `RemoteDesktopWindow` nhận `QImage` và render mượt mà giữ tỉ lệ (aspect ratio), có thanh trạng thái và nút "Ngắt kết nối".
- **Tiêu chí chấp nhận:**
  - [x] Có widget vẽ hình ảnh `QImage` giữ đúng aspect ratio.
  - [x] Có thanh điều khiển: tên máy đang xem, trạng thái, nút ngắt kết nối.
  - [x] Phát signal `disconnectRequested()` khi người dùng bấm ngắt kết nối hoặc đóng cửa sổ.
- **Tệp liên quan:**
  - `clients/RemoteAccessApp/src/GUI/Components/RemoteScreenWidget.h`
  - `clients/RemoteAccessApp/src/GUI/Components/RemoteScreenWidget.cpp`
  - `clients/RemoteAccessApp/src/GUI/Windows/RemoteDesktopWindow.h`
  - `clients/RemoteAccessApp/src/GUI/Windows/RemoteDesktopWindow.cpp`
- **Trạng thái:** [x] Hoàn thành

## Task 5: Tích hợp vòng đời điều khiển trong `DevicesController` và `AppCoordinator`
- **Mô tả:** Kết nối từ khi phiên được kích hoạt (`remoteSessionStarted`) đến khi mở cửa sổ `RemoteDesktopWindow`, nhận frame JPEG $\rightarrow$ giải mã thành `QImage` $\rightarrow$ hiển thị lên UI, và đóng phiên an toàn khi kết thúc.
- **Tiêu chí chấp nhận:**
  - [x] `DevicesController` chuyển tiếp `frameReceived` từ `AdminSessionController` lên coordinator.
  - [x] `AppCoordinator` mở `RemoteDesktopWindow` khi session bắt đầu.
  - [x] Giải mã JPEG thành `QImage` (xử lý an toàn nếu JPEG corrupt) và chuyển cho window vẽ.
  - [x] Khi đóng window hoặc bấm nút disconnect: gửi lệnh kết thúc phiên và dọn dẹp view.
- **Tệp liên quan:**
  - `clients/RemoteAccessApp/src/Controllers/DevicesController.h`
  - `clients/RemoteAccessApp/src/Controllers/DevicesController.cpp`
  - `clients/RemoteAccessApp/src/Controllers/AppCoordinator.h`
  - `clients/RemoteAccessApp/src/Controllers/AppCoordinator.cpp`
- **Trạng thái:** [x] Hoàn thành

## Task 6: Kiểm thử toàn diện & Cập nhật tài liệu
- **Mô tả:** Kiểm tra build, chạy ctest, cập nhật tài liệu `docs/protocol/09_IMPLEMENTATION_STATUS.md`, và tạo commit rõ ràng.
- **Tiêu chí chấp nhận:**
  - [x] Build thành công 100%.
  - [x] Tất cả ctest đều pass.
  - [x] `docs/protocol/09_IMPLEMENTATION_STATUS.md` được cập nhật.
- **Trạng thái:** [x] Hoàn thành
