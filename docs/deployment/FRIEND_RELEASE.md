# Đưa RemoteAcssec lên Internet và phát app cho bạn bè

Cập nhật: 20/11/2026. Mốc mã đã kiểm tra trên Windows VM: `4b61ed21dd9a7ef0f67c84e7947fd3c0721e7ce7`. Khi triển khai, lấy `main` mới nhất và ghi lại commit thực tế.

## Đích cần đạt

Một VM Ubuntu ở Singapore chạy PostgreSQL, Redis, hai Management API, hai Relay và Nginx. API đi qua HTTPS 443; hai Relay dùng TLS trên TCP 9091/9092. Cùng một bản app Windows cho ADMIN và CHILD, đã kèm DLL Qt và có địa chỉ server trong `Open-RemoteAccess.cmd`. Bạn bè giải nén cả thư mục, mở file này, đăng nhập và bật chia sẻ khi muốn.

Không gửi `infrastructure/.env`, khóa TLS riêng, khóa SSH riêng, mật khẩu ADMIN hoặc token vào gói Windows. Client cần chứng chỉ công khai hợp lệ; không dùng `REMOTE_ALLOW_HTTP=1` hay `REMOTE_RELAY_TLS=off` trên Internet.

## Chọn máy chủ theo ngân sách

| Phương án | Tài nguyên và giá công bố | Đánh giá cho người dùng Việt Nam |
|---|---|---|
| [Oracle Cloud Always Free, Singapore](https://docs.oracle.com/en-us/iaas/Content/FreeTier/freetier_topic-Always_Free_Resources.htm) | Tối đa 2 OCPU ARM, 12 GB RAM trong hạn mức miễn phí; cần thẻ để xác minh tài khoản. | **Ưu tiên thử trước**: ở gần Việt Nam và đủ RAM cho stack. Có thể tạm hết chỗ; chọn Home Region Singapore ngay lúc đăng ký vì không đổi được về sau. |
| [Hetzner CX23, châu Âu](https://docs.hetzner.com/general/infrastructure-and-availability/price-adjustment/) | 2 vCPU, 4 GB RAM, 40 GB; $6,49/tháng + [$0,60 IPv4](https://docs.hetzner.com/cloud/servers/primary-ips/overview/), chưa thuế/phí thanh toán. | Trong mức 12 USD, nhưng đường truyền Việt Nam–châu Âu có thể khiến điều khiển màn hình chậm. Chỉ chọn sau khi đo thực tế hoặc chấp nhận độ trễ. |
| [DigitalOcean Basic, Singapore](https://www.digitalocean.com/pricing/droplets) | 2 vCPU, 4 GB RAM: $24/tháng theo bảng giá hiện tại. | Gần Việt Nam nhưng vượt ngân sách. |

Giá và tồn kho có thể thay đổi khi đặt. Không mua VPS 1 GB để chạy cả 7 container; đó không phải cấu hình đã được kiểm thử cho stack này. Tên miền phụ [Duck DNS](https://www.duckdns.org/) miễn phí cho giai đoạn đầu; chọn một tên `ten-ban.duckdns.org`, trỏ IPv4 của VM tới tên đó. Nếu đổi IP, cập nhật DNS trước khi cấp lại chứng chỉ.

## Bảng thực hiện

| Bước | Ai làm | Thao tác | Bằng chứng hoàn thành |
|---|---|---|---|
| 1. Tài khoản | Chủ dự án | Tạo Oracle Free Tier, chọn Home Region Singapore, tự nhập email/điện thoại/thẻ và chấp nhận điều khoản. | Vào được OCI Console; không gửi thông tin đăng nhập cho người triển khai. |
| 2. VM | Chủ dự án cùng người triển khai | Tạo Ubuntu 24.04 ARM64, A1.Flex Always Free 2 OCPU/12 GB, ổ khởi động 50 GB, IPv4 công khai; nhập **khóa SSH công khai**. | SSH được bằng khóa riêng; `uname -m` trả `aarch64`. |
| 3. Mạng | Người triển khai | Mở TCP 80, 443, 9091, 9092 trong OCI và firewall VM. Giới hạn SSH 22 theo IP quản trị nếu khả thi. | Kết nối được từ mạng ngoài; DB 5432, Redis 6379 và API nội bộ 9090 không lộ công khai. |
| 4. DNS | Chủ dự án cùng người triển khai | Tạo tên phụ Duck DNS, trỏ A record đến IPv4 của VM. | `dig +short ten-ban.duckdns.org` ra đúng IP. |
| 5. Nền tảng | Người triển khai | Cài Docker Engine và Compose plugin từ [hướng dẫn chính thức cho Ubuntu](https://docs.docker.com/engine/install/ubuntu/), cùng Nginx/Certbot; clone repo tại commit đã chọn. | `docker compose version`; `git rev-parse HEAD` đúng commit. |
| 6. Bí mật và TLS | Người triển khai | Tạo ba secret độc lập cho JWT, relay và PostgreSQL trong `infrastructure/.env`; cấp chứng chỉ Let's Encrypt cho DNS, gắn cert/key cho cả hai relay. | `docker compose ... config --quiet` đạt; chứng chỉ có SAN khớp DNS; không có secret trong Git/gói client. |
| 7. Server | Người triển khai | Chạy Compose với `docker-compose.yml` và `docker-compose.tls.yml`; cài [mẫu Nginx công khai](../../infrastructure/vps/nginx-site.conf.template) chuyển HTTPS 443 tới HTTP loopback 8080 và chặn `/api/v1/auth/register` từ Internet. | 7 container healthy/running; API HTTPS và relay TLS 9091/9092 truy cập được từ ngoài. |
| 8. Tài khoản | Chủ dự án | Tạo ADMIN qua loopback trên VM bằng mật khẩu riêng, sau đó tạo tài khoản CHILD trong app. | ADMIN/CHILD đăng nhập được; bản hiện tại lưu mật khẩu bằng PBKDF2. |
| 9. Gói Windows | Người triển khai | Build từ cùng commit; chạy `clients/RemoteAccessApp/package_windows.ps1` với tên DNS thật. | ZIP có EXE, DLL Qt, `platforms/qwindows.dll`, `tls/qschannelbackend.dll` và launcher HTTPS/dynamic/TLS. |
| 10. Nghiệm thu | Chủ dự án cùng bạn bè | Thử hai máy Windows ở hai mạng khác nhau: CHILD bật chia sẻ, ADMIN xem và điều khiển, CHILD dừng chia sẻ, kết nối lại. | Thử thực tế đạt, không lỗi chứng chỉ; sau đó mới gửi ZIP cho người dùng khác. |
| 11. Vận hành | Chủ dự án | Giữ bản sao DB, `.env` và khóa TLS ở nơi riêng; theo dõi dung lượng, RAM, log, hạn chứng chỉ. | Có bản sao khôi phục được và biết commit đang chạy. |

## Lệnh triển khai khi đã có VM và DNS

Đăng nhập bằng khóa riêng trên máy quản trị; không chép khóa riêng vào VM:

```sh
ssh -i /home/duongnguyen/.cache/remoteacssec-deploy/id_ed25519 ubuntu@<PUBLIC_IP>
```

Trên VM, sau khi cài Docker, Nginx và Certbot:

```sh
git clone https://github.com/DuongNguyen0210/RemoteAcssec.git /opt/remoteaccess
cd /opt/remoteaccess
git checkout main
git pull --ff-only
git rev-parse HEAD # Ghi lại commit đang triển khai để có thể quay lại.
cp infrastructure/.env.vps.example infrastructure/.env
chmod 600 infrastructure/.env
# Điền JWT_SECRET, RELAY_NODE_TOKEN, POSTGRES_PASSWORD bằng ba giá trị
# riêng từ `openssl rand -hex 32`; đặt RELAY_1/2_ADVERTISED_HOST bằng DNS.
# Đặt các đường dẫn RELAY_1/2_TLS_CERT và RELAY_1/2_TLS_KEY sau khi cấp cert.
docker compose --env-file infrastructure/.env \
  -f infrastructure/docker-compose.yml -f infrastructure/docker-compose.tls.yml config --quiet
docker compose --env-file infrastructure/.env \
  -f infrastructure/docker-compose.yml -f infrastructure/docker-compose.tls.yml up -d --build
docker compose --env-file infrastructure/.env \
  -f infrastructure/docker-compose.yml -f infrastructure/docker-compose.tls.yml ps
```

**Không chạy khối Compose trước khi hoàn tất TLS và Nginx host.** API trong Compose chỉ mở HTTP loopback; host Nginx phải cấp HTTPS cho người dùng. HTTP-01 của Let's Encrypt cần cổng 80 truy cập được để cấp/gia hạn chứng chỉ. Xem thêm [hướng dẫn cấu hình VPS](LOCAL_TO_VPS.md) và [TLS cho Relay](../security/RELAY_TLS.md). Tuyệt đối không dùng `docker compose down -v` khi có dữ liệu người dùng.

Mẫu `infrastructure/vps/nginx-site.conf.template` dành cho Nginx **trên VM**, không phải Nginx trong Compose. Thay `__DOMAIN__` bằng tên Duck DNS thật, cài nó vào `/etc/nginx/sites-available/`, bật site, chạy `nginx -t` rồi cấp HTTPS bằng Certbot `--nginx --redirect`. Mẫu giữ SSE không bị buffer, giới hạn tốc độ đăng nhập và chặn đăng ký ADMIN cùng `/internal/` từ Internet. Kiểm tra đăng ký công khai trả 404 trước khi tạo ADMIN qua `http://127.0.0.1:8080` ngay trên VM. Sau khi cấp chứng chỉ, chuyển khóa relay sang PKCS#8 cho UID 10001 và thiết lập hook gia hạn trước khi chạy Compose.

## Đóng gói Windows sau khi có DNS thật

Trên Windows VM đã build client, mở PowerShell; thay `ten-ban.duckdns.org` và đường dẫn Qt/build bằng đường dẫn thực tế:

```powershell
& .\clients\RemoteAccessApp\package_windows.ps1 `
  -Domain ten-ban.duckdns.org `
  -Executable C:\path\to\build\RemoteAccessApp.exe `
  -QtBin C:\path\to\Qt\bin `
  -MingwBin C:\path\to\Qt\Tools\mingw1310_64\bin `
  -OutputDirectory C:\path\to\RemoteAccess-friends
```

Gửi `RemoteAccess-friends.zip`, không gửi riêng EXE. Launcher trong ZIP đặt `REMOTE_API_URL=https://...`, `REMOTE_RELAY_MODE=dynamic`, `REMOTE_RELAY_TLS=required`. Người nhận giải nén rồi mở `Open-RemoteAccess.cmd`. Windows có thể cảnh báo với ứng dụng chưa ký số; giải pháp phát hành lâu dài là ký mã bằng chứng chỉ code signing. Không hướng dẫn người nhận bỏ qua cảnh báo TLS.

## Trạng thái hiện tại

| Đã kiểm chứng | Còn thiếu trước khi phát cho bạn bè |
|---|---|
| Commit `4b61ed2`; Maven server test đạt; Compose+TLS `config --quiet` đạt với cấu hình thử; client Windows build và 2 bài kiểm thử liên quan đạt; `windeployqt` đã tạo ZIP mẫu 17,5 MB và có backend TLS Windows. Các image nền Docker đều có bản `linux/arm64`. Mẫu Nginx host đã qua `nginx -t` và kiểm tra 404 trên container thử. | Tài khoản Oracle/VM Singapore, tên Duck DNS, chứng chỉ thật, stack chạy công khai, bản ZIP cấu hình DNS thật và nghiệm thu từ hai mạng. ZIP mẫu dùng `example.com`, **không phát cho bạn bè**. |
