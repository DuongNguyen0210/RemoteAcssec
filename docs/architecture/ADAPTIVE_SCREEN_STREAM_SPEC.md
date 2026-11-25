# Đặc tả chọn độ phân giải, chất lượng JPEG và FPS cho luồng màn hình

**Trạng thái** Đã tích hợp vào luồng gửi CHILD → relay → ADMIN.

## Mục tiêu

Khi ADMIN điều khiển từ xa, ảnh cần giữ được chi tiết để đọc chữ và nhận ra các phần tử giao diện. CHILD chọn cấu hình truyền theo thứ tự **độ phân giải → chất lượng JPEG → FPS**, trong ngân sách đường truyền và thời gian xử lý của phiên hiện tại.

## Hiện trạng và giả định

- [ScreenEncoder](../../clients/RemoteAccessApp/src/Streaming/ScreenEncoder.cpp) dùng cấu hình lần gửi trước làm điểm bắt đầu. [ScreenStreamRate](../../clients/RemoteAccessApp/src/Streaming/ScreenStreamRate.cpp) tính ngân sách byte và thời điểm gửi tiếp theo từ tốc độ cấu hình. ACK chỉ mở cửa sổ gửi và cung cấp số đo độ trễ. [ScreenStreamSender](../../clients/RemoteAccessApp/src/Streaming/ScreenStreamSender.cpp) kiểm tra lại ngân sách trước khi gửi JPEG qua phiên hiện tại.
- `REMOTE_STREAM_KBPS` tiếp tục là trần cấu hình, mặc định 1.000 kbps. Giới hạn mỗi khung RDTP là 256 KiB tính cả header và metadata. Với packetizer hiện tại, một khung trong giới hạn này cần 40 byte overhead.
- Lưới ứng viên gồm tỷ lệ `100%, 95%, …, 10%` của ảnh nguồn, chất lượng JPEG `60, 55, 50, 45`. Bộ gửi giãn nhịp theo kích thước khung, tối đa 10 FPS. Tỷ lệ dùng chung cho hai chiều, có làm tròn theo pixel và gộp các kích thước trùng nhau. Bản đầu dùng số pixel làm thước đo độ phân giải và tham số JPEG làm thước đo chất lượng.
- Khi ACK chậm, để cửa sổ một khung điều phối nhịp. Người dùng chấp nhận khoảng trễ 1–2 giây để giữ độ nét. Nhịp thực tế phụ thuộc đường truyền, thời gian mã hóa và tốc độ xác nhận của ADMIN. Màn hình tĩnh theo nhịp refresh riêng.

## Mô hình tối ưu

Với ảnh nguồn và điều kiện phiên tại thời điểm chọn, đặt `x = (scale, jpegQuality, targetFps)`. Tối đa hóa bộ sau theo thứ tự từ điển trong tập cấu hình khả thi.

```text
maximize (encodedWidth × encodedHeight, jpegQuality, targetFps)
```

Ví dụ, nếu cả hai đáp ứng ngân sách byte, `1920×1080 / JPEG 45 / 2 FPS` được ưu tiên hơn `1600×900 / JPEG 60 / 8 FPS`. Ở cùng độ phân giải, `JPEG 55 / 3 FPS` được ưu tiên hơn `JPEG 50 / 8 FPS`. Ở cùng độ phân giải và chất lượng, chọn FPS cao nhất khả thi.

Một ứng viên ở chế độ thường phải thỏa các điều kiện trong bảng.

| Điều kiện | Cách kiểm tra |
| --- | --- |
| `W ≤ 256 KiB` | `W` là tổng byte RDTP của khung, đo từ JPEG thực tế và overhead của packetizer |
| `W ≤ B × 1 s` | `B` bằng `REMOTE_STREAM_KBPS × 125` byte RDTP mỗi giây |
| `sendIntervalMs ≥ max(100, ceil(W × 1000 / B))` | Khoảng gửi còn chịu cửa sổ ACK, thời gian chụp và mã hóa |

Sau khi mã hóa, kiểm tra lại ngân sách với kích thước thực tế của từng khung trước khi gửi. FPS là mục tiêu điều phối. FPS người dùng nhận được đo bằng số khung mới giải mã thành công ở ADMIN trong một cửa sổ thời gian.

## Cách tìm ứng viên

### Duyệt đối chứng

Dùng thuật toán tham chiếu trên cùng ảnh và cùng điều kiện phiên. Duyệt kích thước từ lớn xuống nhỏ, ở mỗi kích thước duyệt chất lượng từ cao xuống thấp. Đo `W`. Cặp đầu tiên vừa ngân sách mỗi khung cho nghiệm tối ưu trong tập rời rạc đã khai báo. Dùng cách này làm mốc kiểm thử trên ảnh cố định.

### Chọn trong lúc truyền

1. Bắt đầu từ cấu hình gần nhất đã gửi thành công trong phiên, hoặc thử độ phân giải gốc khi chưa có lịch sử. Mã hóa lại từ ảnh chụp hiện tại. Dùng kết quả cũ để chọn ứng viên thử trước.
2. Dùng chia đôi khoảng tỷ lệ ở chất lượng tối thiểu để tìm nhanh vùng có khả năng vừa ngân sách. Thử các mức kích thước lân cận, ưu tiên phía có nhiều pixel hơn. Tại một kích thước, thử chất lượng cao hơn khi ngân sách cho phép. Với `W` cố định, tính trực tiếp khoảng gửi tối thiểu từ số byte và tốc độ cấu hình.
3. Phán quyết vừa ngân sách dựa trên byte đã mã hóa. Việc một tỷ lệ thất bại chỉ xác định kết quả của tỷ lệ đã thử. Ứng viên chưa đo vẫn có thể được thử ở lượt sau. Khi hết ngân sách tìm kiếm, chọn cấu hình khả thi có thứ tự ưu tiên cao nhất trong các ứng viên đã kiểm chứng cho ảnh hiện tại. Kết quả trực tuyến là xấp xỉ nghiệm đối chứng.
4. Giới hạn **3 lần mã hóa cho một ảnh chụp** trong vòng gửi trực tuyến, tính cả lần thử để phục hồi. Nếu khung đầu tiên của ảnh quá lớn, dành ít nhất một lượt cho cấu hình nhẹ hơn. Khi hết lượt mà vẫn vượt giới hạn, bỏ ảnh cũ và thử lại với ảnh mới ở nhịp hợp lệ tiếp theo. Chỉ có một tác vụ mã hóa hoạt động cho mỗi sender. JPEG đã hợp lệ được dùng trực tiếp để gửi.
5. Chỉ giảm kích thước khi các mức JPEG đã thử ở kích thước đó vượt ngân sách, hoặc khi cần gửi cấu hình dự phòng trong số lượt tìm kiếm còn lại. Sau hai giây từ khung đầu tiên, tiếp tục thử các cấu hình cao hơn theo đúng thứ tự ưu tiên.

Sau mỗi lần thử, JPEG được giữ là ứng viên có cặp `(số pixel, chất lượng)` lớn nhất trong các kết quả hợp lệ đã đo trên chính ảnh đang xử lý. Bất biến này đúng lúc bắt đầu với tập rỗng. Mỗi JPEG hợp lệ chỉ thay thế kết quả đang giữ khi có thứ hạng cao hơn. Sender kiểm tra số byte của packet trước khi gửi và dùng kích thước khung đã gửi để tính thời điểm sớm nhất cho lần gửi kế tiếp. Khi cả mức nhỏ nhất cũng thất bại, các khung tiếp theo tiếp tục thử chỉ số khác trên lưới, kể cả lúc chưa có ACK đầu tiên. Điều này cho phép tìm lại một mức khả thi khi quan hệ giữa kích thước ảnh và số byte bị đảo chiều.

### Độ phức tạp thời gian

Gọi `P` là số pixel của ảnh nguồn, `S` là số mức trên lưới tỷ lệ và `Q` là số mức chất lượng JPEG. Một lần thu nhỏ và mã hóa có chi phí tối đa `O(P)` trong mô hình xử lý pixel. Kích thước ảnh và lựa chọn encoder quyết định thời gian thực tế.

| Đường xử lý | Số lần mã hóa JPEG trên mỗi ảnh | Độ phức tạp thời gian |
| --- | --- | --- |
| Encoder trước khi áp dụng đặc tả | 1 lần nếu JPEG mặc định vừa ngân sách. Cận trên 11 lần gồm 2 lần ở kích thước gốc, tối đa 7 lần chia đôi lưới 128 mức, 1 lần thử ảnh 1×1 và 1 lần thử tăng chất lượng | `O(P × (1 + log S))` khi xét lưới có kích thước biến đổi. Với lưới 128 mức cố định, chi phí là `O(P)` |
| Encoder đang dùng khi truyền | 1 lần ở cấu hình ổn định khi JPEG vừa ngân sách và chưa đến kỳ thử nâng. Tối đa `K = 3` lần, tính cả lần thử nâng và dự phòng | `O(K × P)`, tức `O(P)` với `K` cố định |
| Duyệt đối chứng trong kiểm thử | Tối đa `S × Q`, bằng 76 lần với 19 tỷ lệ và 4 mức JPEG | `O(S × Q × P)` |

Tỷ lệ và chất lượng được tính từ chỉ số trên lưới cố định. Mỗi lượt trực tuyến chọn ứng viên kế tiếp từ cấu hình đã dùng và trạng thái tìm kiếm của phiên, với chi phí `O(1)` cho mỗi ứng viên. Sau khi đo `W`, tính khoảng gửi tối thiểu từ pacing bằng một phép chia trong `O(1)`. Dàn các lượt thử còn lại qua những ảnh chụp tiếp theo để giữ trần chi phí theo ảnh.

## Điều phối theo trạng thái phiên

- **Ngân sách.** Mỗi khung có tối đa một giây byte theo `REMOTE_STREAM_KBPS`, giới hạn ở 256 KiB. Khoảng chờ ACK phản ánh cả độ trễ mạng, thời gian truyền và tái lắp tại ADMIN. Ngân sách byte chỉ phụ thuộc cấu hình và kích thước tối đa của khung.
- **Pacing.** Dùng tổng byte RDTP đã chấp nhận gửi để tính thời điểm gửi tiếp theo. Trong một cửa sổ dài `t`, số byte gửi phải `≤ B × t + maxWireFrameBytes`. CHILD chờ khung trước được ACK và socket hết dữ liệu chờ trước khi chụp ảnh mới. [Cửa sổ gửi của CHILD](../../clients/RemoteAccessApp/src/Network/Relay/ChildSessionController.cpp)
- **Phục hồi nhịp gửi.** Khi ACK đến, sender thức dậy ngay và tiếp tục kiểm tra pacing. ACK chậm làm giảm FPS thực tế. Việc thử nâng độ phân giải và chất lượng vẫn tiến triển khi lần gửi mới được phép.
- **Phiên cũ và phiên mới.** Khi peer chưa thương lượng ACK, pacing và backpressure của socket điều phối việc gửi. Khi đổi phiên, xoá cấu hình tìm kiếm và thời điểm pacing. Bỏ kết quả mã hóa hoặc ACK thuộc phiên đã kết thúc.

## Cấu trúc và quy ước mã

Client dùng C++17, Qt Gui, Qt Concurrent và Qt Test. Build bằng CMake, chạy test qua CTest.

| Nơi | Trách nhiệm |
| --- | --- |
| [`src/Streaming`](../../clients/RemoteAccessApp/src/Streaming) | `ScreenEncoder` mã hóa ảnh ứng viên. `ScreenStreamSender` điều phối ngân sách, lựa chọn và nhịp gửi |
| [`src/Network/Relay`](../../clients/RemoteAccessApp/src/Network/Relay) | `ChildSessionController` cung cấp trạng thái cửa sổ gửi và mẫu ACK đã khớp. ADMIN cung cấp chỉ số giải mã để nghiệm thu |
| [`src/Network/Protocol`](../../clients/RemoteAccessApp/src/Network/Protocol) | Packetizer cung cấp cách tính byte RDTP và kích thước ảnh thật tới viewer qua JPEG |
| [`tests`](../../clients/RemoteAccessApp/tests) | Qt Test cho chính sách chọn, pacing, relay, kích thước khung và ánh xạ con trỏ |

Dùng kiểu và tên thể hiện đơn vị như `wireBytes`, `bytesPerSecond`, `targetFps` và `cycleMs`. Ví dụ C++ dưới đây so sánh các ứng viên, với `pixelCount` được tính bằng số nguyên 64 bit.

```cpp
return std::tie(candidate.pixelCount, candidate.jpegQuality, candidate.targetFps)
    > std::tie(current.pixelCount, current.jpegQuality, current.targetFps);
```

## Kiểm thử và tiêu chí nghiệm thu

| Kiểm thử | Điều kiện đạt |
| --- | --- |
| Ưu tiên | Các cặp ví dụ trong mô hình cho đúng kết quả. Ứng viên độ phân giải cao hơn thắng khi còn vừa ngân sách, kể cả khi nhịp gửi chậm hơn |
| Tìm kiếm | Duyệt đối chứng khớp với vét cạn toàn bộ tập. Với bộ giả lập kích thước JPEG có điểm đảo chiều, lượt tìm trực tuyến chọn tốt nhất trong các ứng viên hợp lệ đã thử và loại mọi kết quả vượt ngân sách. Trên các cảnh chữ, cuộn trang và nội dung chi tiết lặp lại ổn định, sau 10 giây mục tiêu là sai lệch tối đa một bước tỷ lệ so với đối chứng, hoặc tối đa một bước JPEG nếu cùng độ phân giải |
| Giới hạn | Mỗi khung có `W ≤ 256 KiB`, kể cả tại ngưỡng vừa đúng và quá một byte. Pacing tuân theo giới hạn cửa sổ byte. Đếm cả lần mã hóa thất bại, thử nâng và dự phòng để xác nhận trần 3 lần mỗi ảnh. Trong ca ổn định, ảnh vừa ngân sách và chưa đến kỳ thử nâng dùng đúng 1 lần mã hóa. Không tạo khung thứ hai đang chờ ACK trong phiên hỗ trợ ACK |
| Thay đổi điều kiện | Thêm 1.200 ms độ trễ tại relay kiểm thử. Số khung nhận giảm và độ phân giải giữ trong một bước lưới của mức ban đầu. Khi bỏ độ trễ, nhịp gửi tăng trở lại |
| Độ trễ và hiển thị | Đo p50/p95 thời gian mã hóa theo ảnh chụp, byte RDTP/s, kích thước JPEG, FPS ở ADMIN và khoảng cách giữa các khung mới. Mục tiêu đề xuất cho 1080p là p95 mã hóa tổng mỗi ảnh chụp ≤ 100 ms trên máy nghiệm thu đã ghi cấu hình. Ghi số đo 4K để đánh giá chi phí. Trong ca chuyển động đủ tài nguyên, ADMIN giải mã ≥ 2 FPS trung bình trong cửa sổ 10 giây |
| Tương thích | Thay đổi độ phân giải khi cuộn vẫn ánh xạ đúng click theo kích thước của khung ADMIN đang xem. Phiên mới hoạt động khi nhận muộn ACK hoặc kết quả mã hóa phiên cũ. Peer không có ACK gửi theo backpressure hiện tại |

Chạy các lệnh sau từ thư mục gốc repository. Lệnh cuối mở client để nghiệm thu với môi trường kiểm thử đã khởi động.

```sh
cmake -S clients -B clients/build -DBUILD_TESTING=ON
cmake --build clients/build -j 4
ctest --test-dir clients/build --output-on-failure -R '^(adaptive_screen_encoder|screen_stream_rate|adaptive_stream_relay|admin_screen_viewer|relay_sessions|protocol_contract)$'
ctest --test-dir clients/build --output-on-failure
git diff --check
./clients/build/RemoteAccessApp/RemoteAccessApp
```

Nghiệm thu desktop dùng môi trường cô lập. Ghi rõ loại cảnh, kích thước màn hình, CPU, phiên bản Qt, tốc độ giới hạn và FPS thực tế. [Quy trình hai máy](../testing/TWO_MACHINE_ACCEPTANCE.md)

## Phạm vi triển khai

- **Bắt buộc** Tái sử dụng packetizer, giới hạn byte và luồng xác nhận phiên. Đo hiệu năng bằng ảnh tổng hợp hoặc desktop kiểm thử. Bảo toàn cập nhật kích thước khung và ánh xạ con trỏ khi độ phân giải đổi.
- **Cần duyệt riêng** Thay codec, sửa định dạng RDTP, thêm thư viện hoặc sửa giao diện cấu hình dành cho người dùng.
- **Giới hạn cứng** Chỉ gửi JPEG hợp lệ trong ngân sách của phiên tương ứng. Mọi bộ đệm và lần thử mã hóa trong vòng gửi có giới hạn. Log kiểm thử chỉ ghi số đo và định danh kỹ thuật cần thiết.

## Kết quả kiểm chứng sau khi tách thời gian chờ ACK

- Bộ test client đạt 13/13. Relay kiểm thử giữ chiều rộng 800 pixel khi thêm 1.200 ms độ trễ, với tối đa một khung chờ ACK. Nhịp gửi tăng lại sau khi bỏ độ trễ. [Kiểm thử luồng qua relay](../../clients/RemoteAccessApp/tests/AdaptiveStreamRelayTest.cpp)
- Mẫu cuộn chữ 1920×1080 ở cấu hình 1 Mbps giữ nguyên độ phân giải khi bài đo cục bộ trì hoãn ACK 1.200 ms. Khoảng cách p95 giữa các khung là 1.223 ms. Qua VPS, toàn bộ khung đo được cũng giữ nguyên độ phân giải. Mẫu đo một chiều từ CHILD gửi đến ADMIN nhận dao động khoảng 2,9–6 giây ở 1 Mbps. Điều này cho thấy đường truyền thực tế vẫn có thể kéo dài thời gian nhận ảnh trên mốc 2 giây.
- Cùng cảnh qua VPS ở `REMOTE_STREAM_KBPS=500`, encoder chọn 1632×918. Thời gian gửi đến nhận khi cuộn có trung vị khoảng 1,75 giây, với một mẫu tới 3,29 giây. Các mẫu cho màn hình đứng yên nằm trong khoảng 1,48–1,97 giây. Giá trị 500 kbps là tùy chọn cho máy chia sẻ trên tuyến đã thử.

## Kết quả kiểm chứng của bản trước khi tách thời gian chờ ACK

- Trên Intel Core i5-12450HX, Qt 6.10.2, dùng JPEG tối đa 62.460 byte và 21 ảnh chụp theo đồng hồ mô phỏng 2 FPS. Cả sáu cảnh chữ, cuộn trang và ảnh nhiều chi tiết ở 1080p và 4K đều kết thúc ở cùng độ phân giải và chất lượng với duyệt đủ 76 cặp. Trong lần đo cục bộ, p95 ở 1080p lần lượt là 5,4 / 7,5 / 6,1 ms. Ở 4K là 6,1 / 24,3 / 16,3 ms. Thời gian bao gồm các lần thử trên một ảnh và kết thúc khi encoder trả kết quả. [Kiểm thử đối chứng](../../clients/RemoteAccessApp/tests/AdaptiveScreenEncoderTest.cpp)
- Trên desktop Qt cô lập, một relay TCP kiểm thử hạ thông lượng từ 4.000.000 xuống 30.000 byte/s rồi khôi phục. ADMIN giải mã 39 khung trong 10 giây đầu. Chiều rộng nhận được đi từ 800 xuống 200 rồi trở lại 800 pixel.
- Phiên ADMIN trên host và CHILD trong VM Ubuntu 24.04 dùng Java relay với TLS bắt buộc. ADMIN xác nhận thay đổi trên ảnh sau click và gõ phím, nhận 25 khung trong 10 giây giữ phiên rồi ngắt kết nối thành công. Bài test capture/input native X11 đạt ở cả tỷ lệ hiển thị 100% và 200%. [Chương trình nghiệm thu](../../clients/RemoteAccessApp/tests/DesktopRelayAcceptance.cpp)
