# RDTP v2: xác thực và chọn phiên máy

v2 giữ header 24 byte, byte order và các message streaming hiện có. Byte `version` là **2**. Thay đổi không tương thích v1 ở REGISTER_HOST và CONNECT_REQUEST.

## REGISTER_HOST (0x01)

Payload: `uint16 tokenLength` + `tokenLength` byte UTF-8 của JWT Child.

- tokenLength: 1..4096.
- flags, header.sessionId, sequenceNumber: 0.
- Relay gọi `GET /api/v1/relay/agent` bằng bearer token; API xác thực session và tài khoản còn tồn tại.
- API trả UUID của phiên đăng nhập; registry dùng UUID đó làm khóa channel.
- REGISTER_ACK (0x02) vẫn một byte 0/1.

## CONNECT_REQUEST (0x03)

Payload: `uint16 tokenLength` + JWT Admin + `uint16 targetLength` + targetAgentSessionId UTF-8.

- targetLength: 36, UUID canonical có dấu gạch ngang.
- flags, header.sessionId, sequenceNumber: 0.
- Không nhận username làm target.
- Relay gọi `/api/v1/relay/targets/{targetAgentSessionId}` để kiểm tra chủ sở hữu, presence và phiên đăng nhập.
- Chỉ sau khi API cho phép, relay mới tạo remote session và gửi SESSION_REQUEST tới đúng channel.

Các length là Big Endian. Payload bị cắt, thừa byte, UTF-8 lỗi hoặc token/target sai đều bị từ chối. HTTP authorization bất đồng bộ, timeout 5 giây, không chặn Netty event loop. Mỗi channel chỉ xử lý một yêu cầu authorization ban đầu tại một thời điểm.

REGISTER_HOST không tạo presence: chỉ heartbeat HTTP tạo dòng Devices. Hai máy cùng tài khoản có hai UUID login khác nhau. Relay kiểm tra lại quyền mỗi 10 giây, đóng channel nếu quyền không còn hợp lệ; mất kết nối một bên đóng bên còn lại để reset phiên.

Xem [sơ đồ Account/Device](../architecture/ACCOUNT_DEVICE_FLOW.md) về TTL, API và cách nâng cấp.
