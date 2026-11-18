# Một nơi đăng nhập cho mỗi tài khoản

Chính sách được chủ dự án chọn: từ chối lần đăng nhập thứ hai, giữ lần đăng nhập hiện tại. Áp dụng cả ADMIN và CHILD, nguyên tử trên Redis dùng chung cho các API; không dựa vào tên máy hoặc chỉ chặn ở UI.

Các yêu cầu nghiệm thu:

1. Hai request login đồng thời cùng tài khoản chỉ một được nhận JWT; request còn lại nhận lỗi ACCOUNT_IN_USE có thông báo tiếng Việt.
2. ADMIN JWT cũng phải có UUID phiên và được server kiểm tra; CHILD không thể cấp hai UUID hoạt động cho hai máy.
3. Desktop duy trì phiên kể cả khi CHILD chưa bật/chủ động dừng chia sẻ. Dừng chia sẻ không phải đăng xuất.
4. Đóng app/đăng xuất giải phóng đúng phiên; request cũ không được xóa hoặc gia hạn phiên mới. Tắt đột ngột có thời hạn phục hồi, không khóa tài khoản mười giờ.
5. Token hết phiên không thể gọi API hoặc giữ quyền relay; loại bỏ dữ liệu hiện diện cũ. Không dùng Redis flush hay xóa dữ liệu DB để chuyển đổi.
6. Test đồng thời và hết hạn bằng Redis thật, luồng login/logout qua API thật, và hồi quy desktop/server. Tài khoản đã mở bằng bản cũ cần đăng nhập lại sau nâng cấp.

## Chẩn đoán ảnh lỗi

`java.net.BindException: Address already in use` nghĩa là không bind được cổng, không phải lỗi mật khẩu hay hai stream. Kiểm tra ngày 2026-11-16 thấy container `cnw-web-1` của dự án khác đang publish `127.0.0.1:8080`. Giữ nguyên container đó. Đổi `RELAY_PORT` sang cổng trống, ví dụ 18080, và đổi `REMOTE_RELAY_PORT` trên cả hai client trong chế độ static. Nếu dùng dynamic, đổi cả cổng quảng bá/publish tương ứng. Launcher `bash server/run-relay-local.sh` dùng mặc định TCP 18080; nếu chạy RelayServer trực tiếp trong IDE, đặt biến `RELAY_PORT=18080` trong Run Configuration. Với client static trên host đặt `REMOTE_RELAY_HOST=127.0.0.1`, `REMOTE_RELAY_PORT=18080`; Windows VM NAT dùng host `10.0.2.2` và cùng cổng 18080. Nếu chạy VM lab qua HTTP/TCP thuần, đặt `REMOTE_ALLOW_HTTP=1` và `REMOTE_RELAY_TLS=off` trong tiến trình client của VM. Launcher không tự thay cấu hình ứng dụng đang chạy. Chỉ khởi động một relay cho một địa chỉ/cổng. Không tự dừng server hoặc VM của người dùng.

## Cơ chế và giới hạn

Redis giữ khóa `auth:login:<role>:<id>` chứa UUID phiên và fingerprint thông tin xác thực. Claim dùng SET NX trong Lua; gia hạn/nhả khóa phải khớp phiên, vì vậy request trễ của máy A không thể gia hạn hoặc xóa khóa mới của máy B. ADMIN và CHILD đều phải có UUID và vượt qua kiểm tra phiên trên các API được bảo vệ. Các bản API phải dùng chung Redis.

Desktop gọi `/api/v1/auth/heartbeat` mỗi 5 giây, riêng với heartbeat hiện diện khi chia sẻ. Mỗi lần xác nhận cho phép hoạt động thêm 60 giây; 30 giây cuối giữ khóa để quyền relay cũ có thời gian được kiểm tra lại và đóng. Khi crash/mất mạng, đăng nhập lại được sau tối đa khoảng 90 giây kể từ lần gia hạn cuối. Đổi mật khẩu vô hiệu token cũ; lần kiểm tra token cũ chuyển khóa sang thời gian chờ tối đa 30 giây, không kéo dài khi token cũ tiếp tục gửi request. Khi đóng ứng dụng bình thường, client dừng điều khiển/chia sẻ trước, gửi auth/logout và chờ tối đa 1,5 giây; server giải phóng đúng UUID ngay khi nhận được. Nếu request logout không tới được server thì áp dụng thời hạn crash.

Không cần migration PostgreSQL hay xóa dữ liệu hiện có: chỉ thêm khóa Redis có TTL. Cần chạy lại API và **cập nhật cả hai client**; phiên/token bản cũ không được tiếp tục dùng. Quyền relay được kiểm tra định kỳ, không cam kết thu hồi mọi packet đang truyền ngay tức thì. Chính sách này ngăn đăng nhập lại bằng mật khẩu, không phải cơ chế ràng buộc JWT với phần cứng.

## Kết quả kiểm thử ngày 2026-11-16

- Maven: 26 test API thông thường và 31 test relay đạt; 5 test tích hợp Redis thật đạt riêng (LoginLeaseRedisIT, PresenceRedisIT).
- Hai tiến trình API cổng 18149/18150 dùng chung PostgreSQL/Redis độc lập: mỗi đợt 16 login đồng thời cho ADMIN/CHILD có đúng một HTTP 200 và 15 HTTP 409 ACCOUNT_IN_USE. Kiểm tra unshare vẫn giữ khóa, logout cho đăng nhập lại ngay, token cũ nhận 401 và chỉ một presence. Kiểm thử thời gian thực 60 giây hoạt động + 30 giây chờ, phục hồi sau crash và chống hồi sinh token cũ đều đạt.
- Desktop Qt: đủ 8 nhóm CTest đạt. SingleLoginDesktopAcceptance chạy offscreen với API thật cho ADMIN/CHILD: Enter gửi login, lần login tiếp bị chặn, timer auth/heartbeat hoạt động khi chưa chia sẻ, hủy coordinator gửi logout và login lại được ngay.
- API CRUD tài khoản, reset mật khẩu/thu hồi token, owner isolation và quota đồng thời đạt. API + Java relay thật kiểm tra hai tài khoản CHILD riêng, từ chối login trùng, kết nối đúng thiết bị, logout và xóa tài khoản đạt.
- Đã cập nhật harness full UI/VM và load balancing theo chính sách một phiên. Chưa chạy lại full UI hai desktop VM trong đợt thay đổi này; không can thiệp VM hoặc dịch vụ đang được người dùng chạy.

Chạy lại bài thử chính trên stack **riêng, có thể xóa được** (hai API dùng chung PostgreSQL/Redis):

```sh
TEST_DISPOSABLE_ACCOUNT_STACK=1 \
ACCOUNT_TEST_API_URLS=http://127.0.0.1:18149,http://127.0.0.1:18150 \
python3 server/tests/single_login_acceptance.py

TEST_DISPOSABLE_ACCOUNT_STACK=1 REMOTE_API_URL=http://127.0.0.1:18149 \
QT_QPA_PLATFORM=offscreen clients/build/RemoteAccessApp/SingleLoginDesktopAcceptance -nocrashhandler
```

Bài thử API tạo tài khoản ngẫu nhiên và mất khoảng 95 giây để kiểm tra TTL thật. Sau nghiệm thu cần xóa stack riêng; không chạy trên DB đang sử dụng.
