# Bảng triển khai VPS và chạy demo PBL4

Nếu server dùng IPv6 công cộng, thực hiện các bước cài đặt trong [hướng dẫn dành cho Ubuntu và IPv6](UBUNTU_SERVER_IPV6.md). Khi tập dượt theo mục 7 dưới đây, dùng lệnh `remoteaccess-compose` thay cho `pbl4`.

**Đề tài:** Xây dựng hệ thống điều khiển máy tính từ xa phân tán ứng dụng kiến trúc Relay Proxy và Load Balancing.

**Mốc đạt để lên báo cáo:** máy ADMIN ở Wi-Fi và máy CHILD ở mạng khác (chẳng hạn 4G) đăng nhập qua HTTPS. Khi CHILD bấm **Bật chia sẻ**, ADMIN nhìn thấy màn hình và gõ được vào trình soạn thảo của CHILD. Khi CHILD bấm **Dừng chia sẻ**, phiên điều khiển kết thúc. Muốn chứng minh phân tải relay trực tiếp, chuẩn bị thêm một CHILD hoặc VM với tài khoản riêng.

## Bảng tiến độ

| Đánh dấu | Thời điểm | Việc cần làm | Bằng chứng trước khi đánh dấu |
|---|---|---|---|
| ☐ | Trước 2–3 ngày | 1. Tạo VPS, trỏ DNS và mở cổng | Tên miền phân giải đúng IPv4 công cộng. SSH được từ máy quản trị. |
| ☐ | Cùng ngày | 2. Cài công cụ và lấy mã nguồn | Docker Compose hỗ trợ `!override`. Đã ghi lại commit để build client và server cùng phiên bản. |
| ☐ | Cùng ngày | 3. Cấp chứng chỉ và cấu hình Nginx | HTTPS tin cậy từ máy ngoài. Có chứng chỉ và khóa relay đọc được bởi UID 10001. |
| ☐ | Cùng ngày | 4. Khởi động hệ thống | 7 container chạy, PostgreSQL, Redis và cả hai API đều healthy. Hai relay có heartbeat. |
| ☐ | Trước 1 ngày | 5. Kiểm tra qua Internet và tạo tài khoản | API trả 401 khi thiếu token. Cả 9091 và 9092 xác minh TLS thành công. |
| ☐ | Trước 1 ngày | 6. Chạy desktop thật và diễn tập mục 7 | Điều khiển được qua hai mạng. Đã lưu bằng chứng cân bằng tải và video dự phòng. |
| ☐ | Trước 30 phút | Kiểm tra lại mục 4–6 ngay trên mạng phòng báo cáo | 2 relay online, tài khoản vào được, máy CHILD đang thức, hotspot dự phòng sẵn sàng. |
| ☐ | Hết buổi | Dừng chia sẻ trên các CHILD, đăng xuất, cất bản sao lưu | Phiên đã kết thúc. Nếu kết thúc đợt demo, đóng các luật truy cập công cộng tại nhà cung cấp. |

## 1. Chuẩn bị máy và mạng

Kịch bản dùng **một VPS Ubuntu Server 24.04 LTS x86-64 mới**, Docker rootful, IPv4 công cộng và một tên miền do bạn quản lý. Có thể bắt đầu diễn tập với 2 vCPU, 4 GiB RAM, 30 GiB SSD. Đây là cấu hình gợi ý cho buổi demo nhỏ. Theo dõi RAM, đĩa trong lần build đầu. Hai API và hai relay chạy trên cùng VPS nên khi tắt cả VPS, toàn bộ dịch vụ sẽ dừng.

Tạo bản ghi DNS loại **A**, ví dụ `remote.example.com → IPv4 của VPS`. Nếu dùng dịch vụ proxy DNS như Cloudflare, chọn **DNS only** để hai cổng TCP relay đi thẳng tới VPS. Chỉ tạo AAAA khi đã cấu hình và thử cả đường IPv6. Từ máy cá nhân, chạy `nslookup remote.example.com` với tên miền thật của bạn.

Nếu triển khai **IPv6 trên máy Ubuntu ở nhà**, lấy IPv6 của card mạng trên máy server bằng `ip -6 addr show scope global`. Dùng địa chỉ còn hiệu lực và không có cờ `temporary`. Nếu server là VM VirtualBox, dùng Bridged Adapter để VM nhận địa chỉ từ mạng nhà. Tạo bản ghi **AAAA** trỏ vào địa chỉ của máy server. Tạo luật trong **IPv6 Firewall** của router cho TCP `80`, `443`, `9091` và `9092` tới máy server. Cả ADMIN và CHILD cần có đường IPv6 nếu tên miền chỉ có bản ghi AAAA. [VirtualBox Bridged Networking](https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/networkingdetails.html#network_bridged) · [Let’s Encrypt qua IPv6](https://letsencrypt.org/docs/ipv6-support/)

| TCP vào VPS | Nguồn được phép | Đích / lý do |
|---|---|---|
| 22 hoặc cổng SSH đã chọn | IP quản trị | SSH bằng khóa. Giữ phiên hiện tại và thử đăng nhập ở cửa sổ thứ hai sau khi đổi firewall. |
| 80 | Internet | Xác minh tên miền HTTP-01 và chuyển sang HTTPS. |
| 443 | Các mạng client tham gia demo | Nginx trên VPS nhận HTTPS, chuyển REST và luồng sự kiện SSE đến `127.0.0.1:8080`. |
| 9091, 9092 | Các mạng client tham gia demo | TCP/TLS tới relay 1 và 2. Đi trực tiếp từ Qt tới relay. |
| 8080, 9090, 5432, 6379 | Nội bộ | Giữ 8080 trên loopback. API, PostgreSQL, Redis liên lạc qua mạng Docker. |

Tạo luật inbound ở **firewall/security group của nhà cung cấp**. Nếu IP client thay đổi, cập nhật dải nguồn hoặc cho phép Internet tới 443/9091/9092 trong cửa sổ demo. Nếu VPS bật UFW, cho phép cùng các cổng cần thiết và giữ luật SSH trước khi áp dụng. Docker có thể chuyển tiếp cổng publish trước khi gói tin tới luật UFW. [Docker và firewall](https://docs.docker.com/engine/network/packet-filtering-firewalls/) · [Xác minh HTTP-01](https://letsencrypt.org/docs/challenge-types/)

## 2. Cài máy chủ và lấy mã nguồn

SSH bằng tài khoản quản trị, chạy `sudo -i` để vào **Bash của root trên VPS**. Các khối ghi “VPS” dưới đây dùng phiên đó. Đổi tên miền ví dụ thành tên thật ngay khi khai báo. Nếu mở phiên SSH mới, khai báo lại `DOMAIN`.

```bash
export DOMAIN=remote.example.com
apt-get update
apt-get install -y ca-certificates curl git nginx certbot python3-certbot-nginx openssl jq netcat-openbsd
install -d -m 0755 /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
chmod a+r /etc/apt/keyrings/docker.asc
cat > /etc/apt/sources.list.d/docker.sources <<EOF
Types: deb
URIs: https://download.docker.com/linux/ubuntu
Suites: noble
Components: stable
Architectures: amd64
Signed-By: /etc/apt/keyrings/docker.asc
EOF
apt-get update
apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
systemctl enable --now docker nginx
docker compose version
git clone --branch "${PBL4_SOURCE_BRANCH:-main}" https://github.com/DuongNguyen0210/RemoteAcssec.git /opt/remoteaccess
cd /opt/remoteaccess
git rev-parse HEAD | tee /root/pbl4-demo-commit
```

Cần Compose từ **2.24.4** cho overlay TLS. Đặt `PBL4_SOURCE_BRANCH` trước khi clone nếu muốn thử một nhánh PR. Chốt cùng commit trên máy build Qt. [Cài Docker từ kho chính thức trên Ubuntu](https://docs.docker.com/engine/install/ubuntu/)

## 3. Cấp TLS cho API và hai relay

**VPS.** Một chứng chỉ cho cùng tên miền dùng được ở cả ba cổng. Nginx trên host kết thúc HTTPS tại 443. Hai tiến trình Java relay tự kết thúc TLS ở 9091 và 9092. Certbot sẽ hỏi email quản trị và xác nhận điều khoản trong lần đầu.

```bash
cat > /etc/nginx/sites-available/pbl4 <<EOF
server {
    listen 80;
    listen [::]:80;
    server_name ${DOMAIN};
    location = /api/v1/auth/register { return 403; }
    location ^~ /internal/ { return 404; }
    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Host \$host;
        proxy_set_header Connection "";
        proxy_buffering off;
        proxy_read_timeout 90s;
    }
}
EOF
ln -sfn /etc/nginx/sites-available/pbl4 /etc/nginx/sites-enabled/pbl4
nginx -t && systemctl reload nginx
certbot --nginx --redirect --cert-name pbl4-demo -d "$DOMAIN"
certbot certificates
```

Bước này cần DNS đúng và cổng 80 truy cập được từ Internet. Chờ Certbot báo thành công mới đi tiếp. Quy tắc 403 dành cho URL đăng ký ADMIN công cộng. Chủ VPS sẽ tạo ADMIN qua loopback ở mục 5. Khi stack chưa chạy, HTTPS tới API có thể trả 502.

Với IPv6, chạy `ss -ltnp 'sport = :443'` sau khi Certbot hoàn tất để xác nhận Nginx có listener IPv6. Nếu chưa có, thêm `listen [::]:443 ssl;` vào khối HTTPS của đúng tên miền, rồi chạy `nginx -t && systemctl reload nginx`.

**VPS.** Tạo bản PEM riêng cho relay. Khóa được chuyển sang PKCS#8 và chỉ cấp quyền đọc cho UID 10001. Hook dùng lại sau mỗi lần gia hạn, kể cả khi Certbot đổi đích của symlink chứng chỉ.

```bash
install -d -m 0700 /opt/pbl4-tls
install -d /etc/letsencrypt/renewal-hooks/deploy
cat > /etc/letsencrypt/renewal-hooks/deploy/50-pbl4-relay <<'HOOK'
#!/bin/sh
set -eu
[ "${RENEWED_LINEAGE:-}" = /etc/letsencrypt/live/pbl4-demo ] || exit 0
umask 077
dir=/opt/pbl4-tls
openssl pkcs8 -topk8 -nocrypt -in "$RENEWED_LINEAGE/privkey.pem" -out "$dir/key.new"
chown 10001 "$dir/key.new"
chmod 0400 "$dir/key.new"
install -m 0444 "$RENEWED_LINEAGE/fullchain.pem" "$dir/fullchain.new"
mv -f "$dir/key.new" "$dir/key.pem"
mv -f "$dir/fullchain.new" "$dir/fullchain.pem"
nginx -t && systemctl reload nginx
if [ -x /usr/local/sbin/pbl4 ] && [ -f /opt/remoteaccess/infrastructure/.env ]; then
    /usr/local/sbin/pbl4 up -d --no-deps --force-recreate relay-proxy-1 relay-proxy-2
fi
HOOK
chmod 0700 /etc/letsencrypt/renewal-hooks/deploy/50-pbl4-relay
RENEWED_LINEAGE=/etc/letsencrypt/live/pbl4-demo /etc/letsencrypt/renewal-hooks/deploy/50-pbl4-relay
systemctl enable --now certbot.timer
certbot renew --dry-run
```

Đợt gia hạn thật sẽ tạo lại hai relay để chúng đọc khóa và chứng chỉ mới. Phiên điều khiển đang chạy sẽ gián đoạn. [Gia hạn và deploy hook của Certbot](https://eff-certbot.readthedocs.io/en/stable/using.html#renewing-certificates)

## 4. Cấu hình và khởi động 7 dịch vụ

**VPS.** Chạy từ repo. Khối Python tạo ba secret độc lập, lấy cấu hình nội bộ từ mẫu của dự án và từ chối ghi đè file đã có. Giữ riêng `infrastructure/.env`; client chỉ cần tên miền và chế độ kết nối.

```bash
cd /opt/remoteaccess
python3 - <<'PY'
import os, secrets
from pathlib import Path
domain = os.environ["DOMAIN"]
if domain.endswith(".example.com") or not domain:
    raise SystemExit("Hãy export DOMAIN thành tên miền thật trước")
base = Path("infrastructure")
config = (base / ".env.vps.example").read_text().splitlines()
values = {k: secrets.token_hex(32) for k in ("JWT_SECRET", "RELAY_NODE_TOKEN", "POSTGRES_PASSWORD")}
for n in (1, 2):
    values[f"RELAY_{n}_ADVERTISED_HOST"] = domain
lines = [f"{key}={values[key]}" if (key := line.partition("=")[0]) in values else line for line in config]
for n in (1, 2):
    lines.extend([f"RELAY_{n}_TLS_CERT=/opt/pbl4-tls/fullchain.pem", f"RELAY_{n}_TLS_KEY=/opt/pbl4-tls/key.pem"])
os.umask(0o077)
with (base / ".env").open("x") as output:
    output.write("\n".join(lines) + "\n")
PY
```

Nếu báo file đã tồn tại, kiểm tra các giá trị hiện hành bằng trình soạn thảo trên VPS trước khi tiếp tục. Đổi mật khẩu trong file sau lần khởi tạo PostgreSQL cần đồng bộ cả tài khoản trong DB.

Để publish relay ra IPv6, đặt `RELAY_PUBLIC_BIND=[::]` trong `infrastructure/.env` và giữ `MANAGEMENT_PUBLIC_BIND=127.0.0.1`. Lựa chọn này publish relay qua IPv6. Nếu client dùng cả IPv4 lẫn IPv6, cấu hình Compose publish hai họ địa chỉ và kiểm tra từng đường. [Docker port publishing](https://docs.docker.com/engine/network/port-publishing/)

```bash
cat > /usr/local/sbin/pbl4 <<'WRAPPER'
#!/bin/sh
set -eu
cd /opt/remoteaccess
exec docker compose --env-file infrastructure/.env \
  -f infrastructure/docker-compose.yml -f infrastructure/docker-compose.tls.yml "$@"
WRAPPER
chmod 0755 /usr/local/sbin/pbl4
pbl4 config --quiet && pbl4 up -d --build --wait --wait-timeout 300
pbl4 ps
pbl4 exec -T redis redis-cli EXISTS relay:node:relay-proxy-1 relay:node:relay-proxy-2
```

Kết quả mong đợi là 2 API, 2 relay, 1 nginx, 1 postgres, 1 redis. Hai API, postgres và redis có healthcheck. Lệnh Redis cuối trả **2** khi cả hai relay đã đăng ký và lease còn sống. Nếu đang khởi động, chờ vài giây rồi kiểm tra lại. `pbl4` dùng đúng hai file Compose cho cả thao tác chạy, xem log, backup và khôi phục.

## 5. Kiểm tra Internet và tạo ADMIN demo

**Từ một máy Linux/macOS ngoài VPS**, dùng tên miền thật. Có thể dùng WSL trên laptop Windows. HTTP 401 cho thấy request đã tới lớp xác thực của API. 404 cho thấy listener công cộng đã chặn đường quản trị relay. Mỗi lệnh OpenSSL phải báo xác minh thành công cho đúng hostname.

```bash
DOMAIN=remote.example.com
curl -sS -o /dev/null -w '%{http_code}\n' "https://$DOMAIN/api/v1/devices"         # 401
curl -sS -o /dev/null -w '%{http_code}\n' "https://$DOMAIN/internal/relay/nodes"  # 404
curl -sS -o /dev/null -w '%{http_code}\n' "https://$DOMAIN/api/v1/auth/register" # 403
for port in 9091 9092; do
    openssl s_client -brief -connect "$DOMAIN:$port" -servername "$DOMAIN" \
        -verify_hostname "$DOMAIN" -verify_return_error </dev/null
done
```

Khi thử IPv6, thêm cờ `-6` vào cả `curl` và `openssl s_client`. Chạy từ mạng ngoài có IPv6 để kiểm tra cả đường qua router.

**VPS, lần đầu.** Tạo ADMIN qua cổng HTTP loopback. Dùng mật khẩu riêng cho demo. API băm mật khẩu trước khi lưu trong PostgreSQL. Bản dump DB vẫn là dữ liệu nhạy cảm. Khối sau nhập mật khẩu ẩn và gửi JSON qua stdin.

```bash
read -rp 'Tên ADMIN demo: ' admin_name
read -rsp 'Mật khẩu ADMIN demo: ' admin_pass
echo
printf '%s' "$admin_pass" | jq -Rs --arg u "$admin_name" '{username:$u,password:.}' | \
  curl -sS -w '\nHTTP %{http_code}\n' -H 'Content-Type: application/json' \
    --data-binary @- http://127.0.0.1:8080/api/v1/auth/register
unset admin_pass
```

Kỳ vọng **201**. Mã **409** nghĩa là tên đã tồn tại. Nếu đây là tài khoản bạn đã tạo, dùng lại trên desktop. **502** cần xử lý ở mục 8 trước khi thử đăng nhập.

## 6. Mở client Qt trên máy ADMIN và CHILD

Dùng bộ ứng dụng desktop đã tập dượt, đầy đủ thư viện Qt và TLS backend. Cả hai vai trò chạy cùng chương trình. Trên Windows, mở CMD trong thư mục chứa EXE và chạy khối này trên **từng máy**. Có thể lưu thành `BaoCao.cmd` cạnh EXE, thay tên miền đúng một lần.

```bat
@echo off
cd /d "%~dp0"
set "REMOTE_API_URL=https://remote.example.com"
set "REMOTE_RELAY_MODE=dynamic"
set "REMOTE_RELAY_TLS=required"
set "REMOTE_RELAY_CA_CERT="
set "REMOTE_ALLOW_HTTP="
RemoteAccessApp.exe
```

Dòng `cd /d "%~dp0"` dành cho file `.cmd`; bỏ dòng này nếu gõ trực tiếp trong CMD. Trên Linux, từ thư mục chứa bản build:

```bash
env -u REMOTE_RELAY_CA_CERT -u REMOTE_ALLOW_HTTP \
  REMOTE_API_URL=https://remote.example.com REMOTE_RELAY_MODE=dynamic \
  REMOTE_RELAY_TLS=required ./RemoteAccessApp
```

Nếu chưa có EXE, mở **terminal của Qt 6 MinGW 64-bit** trên Windows có CMake, Ninja, module Qt SVG và công cụ Linguist. Tại thư mục gốc repo cùng commit với VPS, build và đóng gói trước ngày báo cáo:

```bat
cmake -S clients/RemoteAccessApp -B build-demo -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-demo --target RemoteAccessApp
mkdir demo-client
copy build-demo\RemoteAccessApp.exe demo-client\
windeployqt --release --compiler-runtime demo-client\RemoteAccessApp.exe
```

Chép **cả thư mục** sang máy còn lại và chạy thử trên đúng Windows sẽ mang đi. Launcher cũ của VM có thể đặt địa chỉ local, `static` hoặc `TLS=off`. Hãy khởi động qua launcher VPS. Ứng dụng đọc biến lúc khởi chạy và dùng API để tìm relay.

## 7. Kịch bản thuyết trình 5–7 phút

| Phút | Thao tác trước hội đồng | Dấu hiệu và ý nghĩa |
|---|---|---|
| 0–1 | Cho xem `pbl4 ps`. Chỉ đường đi của kết nối. | HTTPS 443 → Nginx host → Nginx Compose → một trong hai API. Ảnh, chuột và phím đi qua TCP/TLS tới relay mà API cấp. TLS kết thúc tại relay. |
| 1–2 | ADMIN đăng nhập, vào trang tài khoản → **Thêm tài khoản**. Ghi đúng tên CHILD được ứng dụng hiển thị. CHILD đăng nhập ở máy thứ hai, rồi bấm **Bật chia sẻ**. | Thiết bị xuất hiện trên ADMIN. Hai máy cùng chủ động kết nối ra VPS, có thể nằm sau hai router khác nhau. |
| 2–3 | ADMIN mở thiết bị. Trên CHILD mở Notepad hoặc trình soạn thảo, ADMIN gõ một cụm từ đã thống nhất. | Cả phòng thấy cùng nội dung trên hai màn hình. CHILD bấm **Dừng chia sẻ** để chứng minh thu hồi quyền, sau đó bật lại. |
| 3–4 | Chạy khối kiểm tra API bên dưới trên VPS. | Nhiều request giống nhau xuất hiện với hai địa chỉ `upstream=…:9090`. Đây là bằng chứng phân tải ở lớp API. |
| 4–5 | Bật chia sẻ đồng thời ở **hai CHILD khác tài khoản**, giữ cả hai online. Chạy khối đọc tải relay. | Đọc số CHILD đã đăng ký trên từng node sau heartbeat. Khi hai node cùng capacity, ban đầu rỗng, dự kiến mỗi node nhận một CHILD. Một phiên đang hoạt động gắn với một relay. |
| 5–7, tùy chọn | Khi đã diễn tập: dừng relay đang có CHILD bằng `pbl4 stop relay-proxy-1` (đổi tên theo thực tế). Chờ lease cũ hết hạn, bật chia sẻ lại nếu cần, mở lại phiên ADMIN. Chạy `pbl4 start relay-proxy-1` và kiểm tra lại mục 4. | Phiên trên relay bị dừng sẽ gián đoạn. Cho thấy có thể thiết lập lại kết nối qua node còn sống. Lease node trong phiên bản này là 30 giây; tổng thời gian còn phụ thuộc reconnect. |

**VPS — bằng chứng cân bằng tải API.** Nhóm request này không mang token. Log cho thấy API nào thực sự nhận request. So địa chỉ với IP của từng container.

```bash
for i in 1 2 3 4 5 6; do
    curl -sS -o /dev/null "https://$DOMAIN/api/v1/devices"
done
pbl4 logs --since 1m nginx | grep '"GET /api/v1/devices HTTP' | tail -6
for svc in management-api-1 management-api-2; do
    docker inspect -f '{{.Name}} {{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$(pbl4 ps -q "$svc")"
done
```

**VPS — bằng chứng phân bổ relay.** Ngay khi hai CHILD đang chia sẻ, đợi một nhịp heartbeat rồi chạy:

```bash
for node in relay-proxy-1 relay-proxy-2; do
    pbl4 exec -T redis redis-cli --raw GET "relay:node:$node" | \
        jq '{instanceId, advertisedHost, advertisedPort, registeredChildren, activeSessions, capacity}'
done
```

`registeredChildren` là số CHILD đã kết nối vào node. `activeSessions` phản ánh phiên điều khiển. Thuật toán xét số lớn hơn giữa lượt giữ chỗ còn hạn và CHILD đã đăng ký, chia cho capacity. Lưu ảnh chụp hai kết quả lúc diễn tập để trình bày nếu chỉ mang được một CHILD tới lớp. [Thiết kế cấp phát relay của dự án](../architecture/RELAY_LOAD_BALANCING.md)

## 8. Xử lý sự cố và quay về bản đã diễn tập

| Triệu chứng | Kiểm tra theo thứ tự | Cách xử lý |
|---|---|---|
| Certbot lỗi hoặc cả ba cổng timeout | DNS A/AAAA, IP public, security group, UFW, `ss -lnt` | Sửa DNS và cổng 80 trước khi xin lại chứng chỉ. Thử từ hotspot để tách lỗi mạng trường. |
| HTTPS 502, login không tới API | `curl -i http://127.0.0.1:8080/api/v1/devices`, `pbl4 ps`, `pbl4 logs --tail 50 management-api-1 management-api-2 nginx` | Khi loopback trả 401, kiểm tra lại `proxy_pass` trên host. Nếu API chưa healthy, kiểm tra DB, dung lượng đĩa và RAM VPS. |
| Đăng nhập được, CHILD online nhưng không có ảnh | Hai cổng OpenSSL ở mục 5, hostname Redis ở mục 7, `pbl4 logs --tail 50 relay-proxy-1 relay-proxy-2` | Sửa DNS, firewall hoặc đường dẫn PEM. Khởi động lại client đúng launcher. Trên Linux dùng phiên X11 đã thử cho demo nhập liệu. |
| Danh sách thiết bị cập nhật trễ | `/api/v1/devices/events`, `proxy_buffering off`, trạng thái chia sẻ và heartbeat | Giữ luồng SSE qua cả hai Nginx, bật chia sẻ lại trên CHILD, xem log API. |
| Báo tài khoản đang sử dụng | Kiểm tra máy/VM còn giữ phiên | Đăng xuất đúng trên máy cũ, hoặc chờ lease hết rồi đăng nhập. Dùng tài khoản riêng cho CHILD thứ hai. |
| Trường chặn cổng relay | HTTPS hoạt động, thử lại OpenSSL 9091/9092 qua hotspot | Chuyển máy bị chặn sang 4G. Nếu mạng tại phòng không đủ, dùng video đã quay qua hai mạng và bằng chứng log lúc diễn tập. |

**Sao lưu trước khi đổi phiên bản.** Trên VPS chạy:

```bash
install -d -m 0700 /root/pbl4-backups
umask 077
dump="/root/pbl4-backups/remote-$(date +%Y%m%d-%H%M%S).dump"
pbl4 exec -T postgres sh -c 'pg_dump -Fc -U "$POSTGRES_USER" "$POSTGRES_DB"' > "$dump" &&
  pbl4 exec -T postgres pg_restore --list < "$dump" > /dev/null
```

Cất một bản dump, bản sao `.env` và commit đã diễn tập ở nơi riêng có mã hóa ngoài VPS. Lệnh trên kiểm tra mục lục bản dump. Khi cần phục hồi, thử `pg_restore` vào **database mới** rồi kiểm tra đăng nhập. Để dừng rồi chạy lại cùng phiên bản và dữ liệu, dùng `pbl4 stop` rồi `pbl4 start`. Tránh `down -v` vì tùy chọn này xóa volume.

Nếu bản mới lỗi, dùng snapshot VPS đã chụp sau lần diễn tập thành công hoặc triển khai lại commit trong `/root/pbl4-demo-commit` cùng DB tương thích. Hibernate dùng `ddl-auto: update`; chỉ quay về mã nguồn cũ có thể để lại schema mới. Trước giờ báo cáo, giữ nguyên bản build đã diễn tập.
