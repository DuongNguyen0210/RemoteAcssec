# Đóng gói và chạy RemoteAccessApp trên Windows và Linux

Cùng một bản client dùng cho ADMIN và CHILD. Hai script tạo bản Qt 6 Release và lưu ZIP trong `dist/`. Client nhận URL HTTPS của Management API và tìm relay qua API. Build và chạy client độc lập với cấu hình Docker trên máy chủ.

## Cấu hình dùng cho hai gói

Tạo `clients/RemoteAccessApp/.env` từ [mẫu cấu hình](../../clients/RemoteAccessApp/.env.example) nếu máy build chưa có file. Sửa URL thành tên miền hoặc IPv6 thật đã có chứng chỉ tin cậy. Giữ ngoặc vuông cho địa chỉ IPv6 trong URL.

```ini
REMOTE_API_URL=https://[2001:db8::1234]
REMOTE_RELAY_MODE=dynamic
REMOTE_RELAY_TLS=required
```

`2001:db8::1234` là địa chỉ minh họa. File thực tế được giữ cục bộ, được Git bỏ qua và chỉ chứa ba cài đặt công khai. `REMOTE_API_URL` là HTTPS origin, có thể có cổng và dấu `/` cuối. URL trong bản đóng gói chỉ chứa thông tin máy chủ, cổng và giao thức. Khi stage ZIP, script xác thực ba khóa và ghi lại đúng ba dòng cấu hình. Nó đọc file client vừa nêu và từ chối khóa bổ sung.

Khi khởi động, ứng dụng đọc `.env` từ thư mục chứa file thực thi, kể cả khi bạn mở từ một thư mục làm việc khác. Với mỗi khóa, thứ tự ưu tiên là **biến môi trường hệ điều hành → file `.env` cạnh chương trình → mặc định trong mã**. Sau khi giải nén, đổi máy chủ bằng cách sửa `REMOTE_API_URL` trong chính file `.env` trong thư mục phát hành rồi mở lại ứng dụng. Nếu đã từng đặt `REMOTE_API_URL` trong môi trường, hãy cập nhật hoặc gỡ biến đó để chỉnh qua file.

Cả hai script bật `REMOTEACCESS_REQUIRE_TLS=ON`. Bản phát hành yêu cầu HTTPS cho API, TLS từ 1.2 cho relay và chứng chỉ hợp lệ với địa chỉ kết nối. Chế độ HTTP hay relay TCP thường dùng trong lab dành cho build phát triển. Windows dùng Schannel và kho CA của Windows. Linux dùng plugin OpenSSL và kho CA của hệ thống. Với URL dùng địa chỉ IP, chứng chỉ cần chứa chính IP đó trong SAN.

## Windows x64

Máy **build** cần Windows 10/11 x64, Qt 6 MinGW 64-bit có Qt SVG và Linguist, MinGW tương ứng với bộ Qt, CMake và `tar.exe` của Windows. Đặt CMake vào `PATH` trước khi chạy. Ví dụ trong PowerShell tại thư mục repo:

```powershell
Copy-Item .\clients\RemoteAccessApp\.env.example .\clients\RemoteAccessApp\.env
notepad .\clients\RemoteAccessApp\.env
```

Chỉ chạy `Copy-Item` khi chưa có `.env` client. Sau khi lưu địa chỉ thật, thay hai đường dẫn SDK trong lệnh sau:

```powershell
.\scripts\package-windows.ps1 -QtBin 'C:\Qt\6.10.2\mingw_64\bin' -MingwBin 'C:\Qt\Tools\mingw1310_64\bin'
```

Script build bằng CMake/MinGW, chạy `windeployqt` và kiểm tra DLL Qt, plugin nền tảng Windows, JPEG, Schannel và runtime GCC/MinGW. Kết quả là **`dist/RemoteAccessWin.zip`**.

Máy người dùng giải nén **cả thư mục**, rồi mở **`RemoteAccessApp.exe`**. DLL, thư mục plugin, `qt.conf` và `.env` phải nằm cùng bộ phát hành. Bản ZIP không yêu cầu Qt SDK hoặc PowerShell để chạy ứng dụng. EXE hiện chưa có chữ ký Authenticode. [Qt for Windows Deployment](https://doc.qt.io/qt-6/windows-deployment.html)

## Linux x86_64

Bản được kiểm thử build trên Ubuntu 24.04 x86_64 với Qt 6.4.2 và glibc 2.39. Gói portable là thư mục giải nén và không cần FUSE. Khi phát hành cho Ubuntu 24.04, build trên Ubuntu 24.04 để tránh phụ thuộc glibc từ bản phân phối mới hơn. Dùng đúng kiến trúc của máy đích.

Trên máy **build** Ubuntu 24.04, từ gốc repo:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build qt6-base-dev qt6-svg-dev \
  qt6-l10n-tools qt6-qpa-plugins libx11-dev libxtst-dev libssl3t64 ca-certificates zip unzip
./scripts/package-linux.sh
```

Script tìm Qt qua `qmake6`, build Release, thu thập các thư viện Qt và phụ thuộc, plugin XCB, OpenSSL, JPEG, SVG và thư viện XTest cho chuột/phím. Có thể đặt `QT_QMAKE` để chọn một SDK cụ thể, `BUILD_DIR` để dùng cache riêng và `BUILD_JOBS` để đặt số tiến trình biên dịch. Kết quả là **`dist/RemoteAccessLinux.zip`**.

Trên máy người dùng **có desktop X11**:

```bash
unzip RemoteAccessLinux.zip
cd RemoteAccessLinux
./run-remoteaccess.sh
```

Launcher chọn plugin XCB và nạp các thư viện từ thư mục `lib/` của gói. Giữ file `.env` cạnh ELF `RemoteAccessApp`. Máy người dùng cần glibc tương thích, X11, font, kho CA hệ thống và thư viện đồ họa hệ thống. Nếu một máy Ubuntu 24.04 tối giản thiếu các thư viện đồ họa hoặc CA, cài các gói runtime sau:

```bash
sudo apt-get install -y ca-certificates libgl1 libegl1 libopengl0 fonts-dejavu-core unzip
```

Với thiết bị CHILD Linux, chọn phiên **Xorg/X11** để điều khiển chuột và bàn phím. XWayland có thể mở giao diện, còn điều khiển toàn bộ desktop Wayland nằm ngoài hỗ trợ hiện tại. [Qt for Linux Deployment](https://doc.qt.io/qt-6/linux-deployment.html)

## Kiểm tra trước khi gửi ZIP

1. Kiểm tra cả hai ZIP giải nén được. Mở `.env` ở mỗi thư mục phát hành và đối chiếu ba khóa với file client của máy build.
2. Thử mở ứng dụng bằng đường dẫn mặc định của từng gói trên máy chưa cài SDK. Windows mở EXE trực tiếp. Linux chạy launcher trong phiên X11.
3. Từ một máy có IPv6, thử HTTPS và danh tính chứng chỉ relay theo mục 8 của [hướng dẫn IPv6](UBUNTU_SERVER_IPV6.md#8-kiểm-tra-từ-internet-qua-ipv6). Với Internet kiểm tra từ chính mạng mà ADMIN và CHILD sẽ dùng.
4. Đăng nhập ADMIN và CHILD bằng tài khoản đã được cấp. Xác nhận CHILD mặc định tắt chia sẻ. Trên CHILD chọn **Bật chia sẻ**, rồi kết nối từ ADMIN. Kiểm tra hình ảnh và thao tác trên một cửa sổ thử nghiệm. Trên CHILD chọn **Dừng chia sẻ** và xác nhận phiên kết thúc.
5. Khi chia sẻ gói, giữ cả thư mục sau giải nén và nhớ rằng HTTPS origin trong `.env` phải khớp chứng chỉ máy chủ. Hai gói kèm thông báo giấy phép cho các runtime phân phối cùng ứng dụng.

Có thể chạy kiểm tra cấu hình và chính sách TLS từ mã nguồn. `TEST_PUBLIC_IPV6` là tùy chọn, lấy IP trần từ `.env` khi đang thử máy chủ thật. Test gửi GET ẩn danh và mở TLS tới hai relay, không cần tài khoản.

```bash
cmake -S clients/RemoteAccessApp -B build-release/check \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DREMOTEACCESS_REQUIRE_TLS=ON
cmake --build build-release/check --target ClientConfigTest SecurityTransportTest
ctest --test-dir build-release/check --output-on-failure -R '^(client_config|security_transport)$'
```

Xem môi trường kiểm thử, kết quả và danh sách file trong [báo cáo phát hành](CLIENT_RELEASE_REPORT.md).
