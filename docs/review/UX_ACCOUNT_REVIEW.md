# UX và quản lý tài khoản

Mục tiêu: giữ phối màu, bỏ thao tác hạ tầng/không có tác dụng khỏi UI, thu gọn quản lý tài khoản và kiểm chứng tạo/sửa/xóa.

## Quyết định giao diện

- Bỏ địa chỉ API và nút lưu/khôi phục server khỏi Cài đặt. `REMOTE_API_URL` là cấu hình triển khai; địa chỉ từng lưu bằng QSettings được bỏ qua để không chặn launcher mới.
- Bỏ chất lượng JPEG và giữ tỉ lệ trong Cài đặt vì các giá trị chỉ được lưu, không được luồng phát/viewer sử dụng. Giữ hành vi thích ứng chất lượng và giữ tỉ lệ đang hoạt động.
- Bỏ email chưa được API cung cấp và thông tin kỹ thuật RDTP/SSE/Phase khỏi UI. Cài đặt còn thông tin tài khoản và đổi mật khẩu có trạng thái đang gửi.
- Bỏ mật khẩu giả/nút mắt không có xử lý khỏi thẻ tài khoản. Giữ tên đăng nhập thật và thao tác chép, sửa, xóa. Phần đếm tài khoản được thu gọn. Ô tìm thiết bị chỉ hiển thị ở trang Thiết bị.
- Tạo tài khoản hiển thị trước tên đăng nhập đầy đủ gồm prefix ADMIN, giới hạn tên theo tổng 50 ký tự. Sửa tài khoản vẫn cho giữ mật khẩu cũ; khi nhập mật khẩu mới có giải thích CHILD cần đăng nhập lại.
- Giữ nhập CSV vì chức năng có thực; bỏ lựa chọn Excel chưa hỗ trợ, khóa thao tác khi gửi, cho bỏ file đã chọn, gửi từng tài khoản nối tiếp và báo lỗi theo từng tên.
- Các nút Kết nối mới, Làm mới thiết bị, Ngắt kết nối, Nhật ký hiện tại và Hủy vẫn có tác dụng thực tế nên được giữ.

## Lỗi luồng tài khoản đã xử lý

Tạo mới trước đây ghi mật khẩu thô trong khi sửa mật khẩu dùng PBKDF2. Tạo mới nay dùng cùng hàm băm, chuẩn hóa khoảng trắng tên, kiểm tra tổng tên đăng nhập/mật khẩu và từ chối tên trùng ADMIN. HTTP trả 400 cho dữ liệu sai và 409 cho xung đột. Giao dịch tạo khóa hàng owner trước kiểm tra quota để các yêu cầu đồng thời không vượt giới hạn tài khoản. Quyền sửa/xóa và cơ chế thu hồi phiên sau đổi mật khẩu được giữ.

## Kiểm chứng

- Qt tests cho không còn server/stream settings, chép tên đăng nhập, prefix/giới hạn tên, submit một lần, hiển thị lỗi rồi thử lại, sửa mật khẩu được che, request CSV nối tiếp và danh sách/loading/error/search.
- Server tests cho hash khi tạo, trim tên, giới hạn độ dài; các test sửa/đổi mật khẩu/quyền sở hữu hiện có được chạy lại.
- Ảnh kiểm tra ngoài Git tại `/tmp/pbl4-ux-preview/`; đã xem thẻ tài khoản, dialog tạo và trang Cài đặt với QSS hiện tại.
- Script `server/tests/account_management_acceptance.py` đã PASS trên API/PostgreSQL/Redis riêng tại loopback 18139: tạo/trim tên, trùng tên và tên quá dài, đổi tên giữ mật khẩu, đổi mật khẩu và từ chối token cũ, login mới, từ chối owner khác, xóa và tạo đồng thời. Với một tài khoản đang có và quota 5, tám request tạo cùng lúc có đúng bốn thành công và bốn bị từ chối; danh sách có đúng năm tài khoản.
- SQL kiểm tra các tài khoản còn lại của lượt thử trước khi dọn: mọi mật khẩu CHILD đều có tiền tố PBKDF2. Không sửa dữ liệu phát triển và không thêm schema migration cho thay đổi này.
- Bản build desktop hoàn tất; cả bảy CTest executables PASS (35.41 giây). `AccountDeviceFlowTest` có 18 lượt PASS gồm init/cleanup; toàn bộ 24 unit tests API PASS, không fail/error/skip.
- Đã dọn ba container, hai volume và network riêng, xóa secrets môi trường thử. Docker stop từ host ban đầu bị từ chối; các dịch vụ thử được dừng bên trong container rồi xóa thành công. Không restart Docker daemon, server local hoặc VM đang dùng của người dùng.
- Có regression bỏ snapshot tài khoản đang tải nếu có refresh mới sau chỉnh sửa, lấy lại dữ liệu một lần để tránh UI hiển thị kết quả cũ. Hai trạng thái CSV đã chọn/bỏ chọn giữ đúng nhãn submit, xóa lỗi cũ và cho quay lại nhập thủ công.

## Phạm vi bản chạy

Các thay đổi nằm trong mã nguồn và bản build desktop trên host. Không tự thay ứng dụng đang chạy hoặc bản cài trong VM Windows của người dùng; cần build/copy bản mới trước khi nghiệm thu UI mới ở Windows. Phối màu QSS không đổi. API URL nay chỉ theo môi trường launcher hoặc mặc định local, không còn ưu tiên giá trị QSettings cũ.
