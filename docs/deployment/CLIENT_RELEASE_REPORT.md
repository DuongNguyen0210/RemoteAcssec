# Báo cáo phát hành client ngày 25/11/2026

## Quyết định triển khai

- Ba tham số công khai được đọc một lần từ `.env` cạnh file thực thi ngay sau khi tạo `QApplication`. Các điểm đọc API, relay ADMIN và relay CHILD dùng chung bộ đọc cấu hình. Biến môi trường hiện có tiếp tục được ưu tiên theo từng khóa.
- Cả hai gói dùng Qt 6 Release với `REMOTEACCESS_REQUIRE_TLS=ON`. API yêu cầu HTTPS và relay yêu cầu TLS. Qt xác minh chuỗi tin cậy và danh tính máy chủ. Windows dùng Schannel. Linux dùng OpenSSL.
- Windows có thể mở EXE trực tiếp. Linux dùng thư mục portable và launcher XCB để giữ hỗ trợ X11. Máy Linux cung cấp glibc và thư viện đồ họa hệ thống.
- Script đóng gói luôn dựng thư mục tạm mới từ danh sách đầu vào. Bộ kiểm tra cấu hình ghi đúng ba biến công khai vào ZIP. File cục bộ theo máy và ZIP nằm ngoài Git.
- Diff Git chỉ nằm ở client, công cụ đóng gói và tài liệu. Với server Ubuntu đang chạy, phép thử dùng GET ẩn danh và bắt tay TLS. Luồng chia sẻ của CHILD tiếp tục yêu cầu người dùng chọn **Bật chia sẻ**.

## Kết quả thực tế

| Kiểm tra | Kết quả |
|---|---|
| Windows x64 | Build Qt 6.10.2 và MinGW 13.1.0 trong Windows 11 VM. `windeployqt` tạo ZIP. EXE mở từ thư mục làm việc khác với PATH chỉ có Windows và thư mục SDK được tạm cất. Kiểm tra module cho thấy Qt Core, Qt Network và plugin nền tảng được nạp từ thư mục giải nén. |
| Linux x86_64 | Build trên Ubuntu 24.04 với Qt 6.4.2. Launcher mở UI X11 trong container Ubuntu 24.04 không có Qt SDK, CMake hay trình biên dịch. `/proc` xác nhận Qt Core và plugin XCB được nạp từ ZIP đã giải nén. |
| IPv6 thật và TLS | Hai runtime Release kết nối tới IPv6 trong `.env`. API trả HTTP 401 cho GET ẩn danh. Cả 9091 và 9092 hoàn tất TLS với danh tính chứng chỉ đúng IP. |
| Xác minh TLS trong lab | Trên cả Windows và Linux, kiểm tra REST/SSE với chứng chỉ hợp lệ, CA không được tin cậy và tên máy chủ sai đạt kỳ vọng. Kiểm tra từ chối ép relay về TCP thường cũng đạt. |
| Cấu hình và nội dung ZIP | Kiểm tra thứ tự ưu tiên, UTF-8 BOM/CRLF, URL IPv6 có ngoặc và bỏ qua các khóa lạ khi đọc. Cả hai ZIP qua kiểm tra CRC, mỗi ZIP chứa đúng ba dòng cấu hình. Bộ tạo gói từ chối trường secret và URL chứa thông tin đăng nhập. |
| Ứng dụng và đầu vào hệ điều hành | CTest đạt 14/14, gồm kiểm tra CHILD có thể bật và dừng chia sẻ. Linux X11 đạt kiểm tra chụp màn hình, click và gõ phím ở 100% và 200%. Windows native đạt ở 100% và ở 200% khi chạy riêng tại 1920×1080. |
| Backend | Maven đạt 76 test Java. |

Ở Windows VM, phép thử native 200% ban đầu vấp sai số một pixel tại độ phân giải có chiều cao lẻ. Lần chạy liền sau bài 100% tại 1920×1080 cũng vấp điều kiện so sánh màu tuyệt đối của fixture. Bản đã đóng gói đạt khi chạy lại riêng ở 200% tại 1920×1080, dùng nguyên các điều kiện kiểm tra. Kiểm tra IPv6 là từ môi trường phát triển và VM. Chưa chạy phiên điều khiển trên hai mạng bên ngoài bằng tài khoản thật.

Mã SHA-256 của hai ZIP tạo trong lượt này:

```text
2179c1a2676cba9e781764f35263fb9c8c24e3df0b1e5295cc47cadd7d895aa1  dist/RemoteAccessWin.zip
524ce89268aac1f12d66a0f692eedb157831d483295fe02bffcdeabe8915b403  dist/RemoteAccessLinux.zip
```

## Danh sách file

```text
.gitignore
clients/RemoteAccessApp/.gitignore
clients/RemoteAccessApp/.env.example
clients/RemoteAccessApp/CMakeLists.txt
clients/RemoteAccessApp/src/Main.cpp
clients/RemoteAccessApp/src/Controllers/AppCoordinator.cpp
clients/RemoteAccessApp/src/Network/Http/ApiClient.cpp
clients/RemoteAccessApp/src/Network/Http/TlsConfiguration.h
clients/RemoteAccessApp/src/Network/Relay/AdminSessionController.cpp
clients/RemoteAccessApp/src/Network/Relay/RelayClient.cpp
clients/RemoteAccessApp/src/Network/TransportPolicy.h
clients/RemoteAccessApp/src/Utils/ClientConfig.cpp
clients/RemoteAccessApp/src/Utils/ClientConfig.h
clients/RemoteAccessApp/tests/ClientConfigTest.cpp
clients/RemoteAccessApp/tests/SecurityTransportTest.cpp
scripts/package-windows.ps1
scripts/package-linux.sh
scripts/stage-client-config.cmake
scripts/client-third-party.txt
scripts/licenses/GPL-3.0.txt
scripts/licenses/LGPL-3.0.txt
docs/deployment/CLIENT_RELEASE.md
docs/deployment/CLIENT_RELEASE_REPORT.md
docs/deployment/FRIEND_RELEASE.md
docs/deployment/UBUNTU_SERVER_IPV6.md
```

Script cũ `clients/RemoteAccessApp/package_windows.ps1` đã được thay bằng `scripts/package-windows.ps1`. File cấu hình cục bộ là `clients/RemoteAccessApp/.env`. Hai kết quả phát hành là `dist/RemoteAccessWin.zip` và `dist/RemoteAccessLinux.zip`.
