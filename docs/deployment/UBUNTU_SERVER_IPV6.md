# Cài RemoteAcssec trên Ubuntu Server qua IPv6 công cộng

Hướng dẫn này dùng một Ubuntu Server mới, có quyền `sudo` và có IPv6 công cộng được gán cho chính máy chủ. Ví dụ dùng Ubuntu 24.04 LTS. Bạn có thể dùng tên miền có bản ghi AAAA hoặc dùng trực tiếp địa chỉ IPv6. ADMIN và CHILD chạy ứng dụng Qt trên hai máy có giao diện đồ họa và có đường IPv6 tới server.

| Kết nối | Cổng TCP | Đích |
|---|---:|---|
| HTTP từ Internet | 80 | Nginx trên Ubuntu phục vụ xác minh và gia hạn chứng chỉ, sau đó chuyển các URL thông thường sang HTTPS |
| HTTPS từ ADMIN và CHILD | 443 | Nginx trên Ubuntu → `127.0.0.1:8080` → Nginx Compose → một trong hai Management API |
| Relay TLS từ ADMIN và CHILD | 9091, 9092 | Nginx chuyển tiếp TCP đến relay tương ứng trên loopback. Java relay xuất trình chứng chỉ TLS và xử lý phiên |
| SSH, nếu dùng | Cổng SSH thực tế | Quản trị hệ điều hành, giới hạn nguồn ở router hoặc firewall VPS nếu có thể |
| PostgreSQL, Redis, API Java và HTTP nội bộ | 5432, 6379, 9090, 8080 | Mạng Docker hoặc loopback của Ubuntu |

Chạy từng khối ghi **Ubuntu Server** trong cửa sổ hiện tại. Nếu thấy lỗi, xử lý khối đó trước khi đi tiếp. Các lệnh ghi **máy ngoài** chạy từ một máy khác, tốt nhất qua mạng khác.

## 1. Kiểm tra Ubuntu và lấy IPv6

**Ubuntu Server.**

```bash
cat /etc/os-release
dpkg --print-architecture
ip -br -6 address show scope global
ip -6 addr show scope global
ip -6 route show default
ip -6 route get 2606:4700:4700::1111
timedatectl status
nproc
free -h
df -h /
```

- `cat` và `dpkg` xác định phiên bản Ubuntu và kiến trúc gói APT. `nproc`, `free` và `df` lần lượt cho biết CPU, RAM và đĩa. Với hai API Java, 4 GiB RAM là điểm bắt đầu thực tế cho bản demo.
- `ip -6 addr` cho biết địa chỉ của server. Chọn địa chỉ công cộng còn hiệu lực trên card mạng, ưu tiên địa chỉ ổn định. Bỏ phần `/64` hoặc `/128` khi nhập ở các bước sau. `fe80:…` là link-local, `fc…` và `fd…` là địa chỉ nội bộ, còn cờ `temporary` chỉ địa chỉ có thể thay đổi.
- `ip -6 route` kiểm tra đường đi IPv6. Tuyến đi ra và cột `src` chỉ cho biết cách server gửi gói tin. Đường vào còn phụ thuộc router và firewall. Nếu VM đang ở NAT và thiếu IPv6 công cộng, kiểm tra tùy chọn bridge hoặc cấu hình IPv6 qua hypervisor. Nếu thiếu tuyến trên VPS, lấy cấu hình từ nhà cung cấp.
- `timedatectl` cho biết đồng hồ đã đồng bộ hay chưa. Nếu NTP tắt, dùng `sudo timedatectl set-ntp true` rồi kiểm tra lại. TLS và phiên đăng nhập cần giờ hệ thống chính xác.

### Chọn thông tin client sẽ dùng

- **Có tên miền**. Tạo một bản ghi **AAAA** trỏ về IPv6 của Ubuntu. Bật **DNS only** nếu nhà cung cấp DNS có chế độ proxy HTTP. Tên miền giúp client giữ nguyên URL khi IPv6 đổi, miễn là bạn cập nhật AAAA và vẫn mở đúng firewall. Với cách cài trong tài liệu này, giữ hostname chỉ có AAAA. Một bản ghi A gửi client IPv4 sang đường mạng chưa được cấu hình.
- **Chỉ có IPv6**. Let’s Encrypt có thể cấp chứng chỉ khớp trực tiếp địa chỉ IP. Certbot cần ít nhất phiên bản 5.4 cho webroot và chứng chỉ dùng profile `shortlived` với thời hạn khoảng sáu ngày. Phương án này cần lịch gia hạn hoạt động và IPv6 ổn định. Khi đổi IP, cần cấp lại chứng chỉ và đổi URL phía client. [Chứng chỉ IP của Let’s Encrypt trong Certbot](https://letsencrypt.org/2026/03/11/shorter-certs-certbot/) · [Xác minh tên miền qua IPv6](https://letsencrypt.org/docs/ipv6-support/)

## 2. Cài Git, Nginx, Certbot và Docker

**Ubuntu Server.** Vào Bash của root để các khối ghi file trong `/etc` và `/opt` chạy đúng quyền. Nếu đã ở root, bỏ dòng `sudo -i`.

```bash
sudo -i
```

Khi đã vào Bash của root, chạy khối cài đặt.

```bash
apt-get update
apt-get install -y ca-certificates curl git nginx libnginx-mod-stream \
  openssl jq dnsutils ufw python3 snapd
snap install --classic certbot
/snap/bin/certbot --version
```

- `apt-get update` tải danh mục gói. `apt-get install` cài gói từ kho Ubuntu. Trên máy đang chạy dịch vụ, xem `apt list --upgradable` và chọn thời điểm riêng để nâng cấp hệ điều hành nếu cần.
- `git` lấy mã nguồn. `curl` kiểm tra HTTP và tải khóa APT. `ca-certificates` cung cấp các CA tin cậy. `nginx` xử lý HTTPS, còn `libnginx-mod-stream` cho phép chuyển tiếp relay qua TCP. `openssl` kiểm tra TLS và đổi định dạng khóa relay. `jq` tạo và đọc JSON. `dnsutils` cung cấp lệnh `dig`. `ufw` cấu hình firewall của Ubuntu. `python3` xác nhận IPv6 và tạo cấu hình bí mật.
- Snap cung cấp Certbot phiên bản mới và lịch gia hạn. Nếu Certbot đã được cài bằng APT trên máy đang vận hành, kiểm tra lịch và cấu hình hiện có trước khi đổi cách cài. Các lệnh dưới đây gọi đúng bản Snap bằng `/snap/bin/certbot`. [Hướng dẫn cài Certbot](https://certbot.eff.org/instructions?ws=other&os=snap)

Kiểm tra Docker cũ trước khi thêm kho chính thức.

```bash
dpkg-query -W docker.io docker-compose docker-compose-v2 podman-docker containerd runc 2>/dev/null || true
```

Máy mới thường không có các gói trên. Nếu có Docker đang chạy tải khác, kiểm tra `docker ps` và cách cài của máy trước khi thay bộ gói. Các gói `docker.io`, Compose của Ubuntu, `containerd` và `runc` cài riêng có thể xung đột với gói từ Docker.

```bash
install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
chmod a+r /etc/apt/keyrings/docker.asc
cat > /etc/apt/sources.list.d/docker.sources <<EOF_DOCKER
Types: deb
URIs: https://download.docker.com/linux/ubuntu
Suites: $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}")
Components: stable
Architectures: $(dpkg --print-architecture)
Signed-By: /etc/apt/keyrings/docker.asc
EOF_DOCKER
apt-get update
apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
systemctl enable --now docker nginx
docker version
docker compose version
docker run --rm hello-world
```

- File `docker.sources` chọn kho stable theo codename và kiến trúc thật của máy. `Signed-By` giới hạn khóa ký cho kho này.
- `docker-ce` là daemon, `docker-ce-cli` là lệnh Docker, `containerd.io` chạy container, Buildx build image, Compose quản lý cả stack. Compose phải hỗ trợ `!override` trong overlay TLS, có từ 2.24.4. Những phiên bản mới hơn có thể mang số major cao hơn.
- `systemctl enable --now` khởi động dịch vụ và bật lại sau reboot. `hello-world` kiểm tra daemon có tải và chạy image được. `--rm` xóa container thử nghiệm sau khi lệnh chạy xong.
- Ubuntu cần đường ra tới kho APT, GitHub, registry Docker và Maven Central khi cài/build. Máy chỉ có đường IPv6 ra Internet có thể cần thêm kết nối IPv4, NAT64 hoặc proxy cho các đích mà mạng hiện tại chưa tới được. [Cài Docker Engine trên Ubuntu](https://docs.docker.com/engine/install/ubuntu/) · [Cách Compose gộp file cấu hình](https://docs.docker.com/reference/compose-file/merge/#replace-value)

## 3. Lấy dự án và lưu cấu hình công cộng

**Ubuntu Server.** Khối clone dùng cho lần đầu. Nếu `/opt/remoteaccess` đã tồn tại, kiểm tra thư mục đó và đọc mục cập nhật. Dòng `read` cho phép dùng commit trong PR trước khi merge. Bỏ trống để lấy `main` tại thời điểm clone.

```bash
git clone https://github.com/DuongNguyen0210/RemoteAcssec.git /opt/remoteaccess
cd /opt/remoteaccess
```

```bash
read -rp 'Commit cần cài, hoặc Enter để dùng origin/main: ' DEPLOY_COMMIT
```

```bash
DEPLOY_COMMIT="${DEPLOY_COMMIT:-origin/main}"
git switch --detach "$DEPLOY_COMMIT" &&
  git rev-parse HEAD | tee /root/remoteaccess-source-commit
```

`git clone` tải mã nguồn và các nhánh GitHub. `switch --detach` chọn chính xác một commit. `rev-parse` ghi SHA đang sử dụng để đối chiếu khi cài lại hoặc build client. Dùng cùng SHA trên máy build Qt.

Nhập IPv6 công cộng vừa xác định và tên DNS nếu có. Ví dụ tên miền là `remote.tenmiencuaban.vn`. Enter ở câu thứ hai để dùng IP trực tiếp.

```bash
read -rp 'IPv6 công cộng của Ubuntu, bỏ /64: ' PBL4_IPV6
```

```bash
read -rp 'Tên miền AAAA, hoặc Enter để dùng IP trực tiếp: ' PBL4_DNS
```

```bash
export PBL4_IPV6 PBL4_DNS
python3 - <<'PY'
import ipaddress
import json
import os
import re
import shlex
import subprocess
from pathlib import Path

try:
    ip = ipaddress.IPv6Address(os.environ["PBL4_IPV6"])
except ValueError as exc:
    raise SystemExit("Hãy nhập một IPv6 trần, không có ngoặc, zone ID hoặc độ dài prefix.") from exc
if not ip.is_global or ip.is_multicast or ip.ipv4_mapped or ip.scope_id:
    raise SystemExit("Cần IPv6 unicast công cộng.")
interfaces = json.loads(subprocess.check_output(["ip", "-j", "-6", "addr", "show", "scope", "global"]))
if not any(ip == ipaddress.IPv6Address(addr["local"])
           for nic in interfaces for addr in nic.get("addr_info", [])):
    raise SystemExit("IPv6 này chưa được gán trên Ubuntu Server.")

dns = os.environ["PBL4_DNS"].strip().lower().rstrip(".")
if dns and not re.fullmatch(r"(?=.{1,253}$)(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z]{2,63}", dns):
    raise SystemExit("Nhập tên DNS đầy đủ, không kèm https://, cổng hoặc đường dẫn.")
name = dns or f"[{ip}]"
os.umask(0o077)
with Path("/root/remoteaccess-public.conf").open("x") as output:
    for key, value in (("PBL4_IPV6", str(ip)), ("PBL4_DNS", dns),
                       ("PBL4_HOST", dns or str(ip)), ("PBL4_AUTHORITY", name)):
        output.write(f"{key}={shlex.quote(value)}\n")
PY
```

Script kiểm tra địa chỉ có mặt trên Ubuntu rồi tạo file cấu hình root-only. Chế độ `x` báo lỗi nếu file đã tồn tại. Nếu nhập sai, mở `/root/remoteaccess-public.conf` bằng trình soạn thảo và sửa các giá trị liên quan trước khi đi tiếp. Với IP trực tiếp, `PBL4_HOST` lưu IP trần cho relay, còn `PBL4_AUTHORITY` thêm ngoặc vuông cho URL.

Mỗi lần mở một Bash root mới, nạp lại bốn biến bằng dòng đầu tiên bên dưới. Nếu dùng tên miền, kiểm tra bản ghi từ resolver hiện tại.

```bash
. /root/remoteaccess-public.conf
printf 'IPv6 = %s\nAPI = https://%s\n' "$PBL4_IPV6" "$PBL4_AUTHORITY"
if [ -n "$PBL4_DNS" ]; then
    dig +short AAAA "$PBL4_DNS"
    dig +short A "$PBL4_DNS"
fi
```

`dig AAAA` phải trả đúng IPv6 server. IPv6 có thể được viết dạng nén khác mà vẫn là cùng địa chỉ. Trước khi xin chứng chỉ, chờ DNS cập nhật trên mạng ngoài.

## 4. Mở đúng cổng ở Ubuntu và router/VPS

**Ubuntu Server.** Kiểm tra listener hiện tại và trạng thái UFW.

```bash
ss -ltnp
ufw status verbose
grep '^IPV6=' /etc/default/ufw
```

Nếu UFW sẽ quản lý firewall, file `/etc/default/ufw` cần có `IPV6=yes`. Nếu SSH đang dùng, cho phép **cổng đang lắng nghe thực tế**, rồi giữ cửa sổ SSH hiện tại và thử kết nối ở cửa sổ thứ hai trước khi đóng. Chỉ chạy khối bật UFW dưới đây trên Ubuntu mới hoặc sau khi đã kiểm tra các luật và dịch vụ có sẵn.

```bash
read -rp 'Cổng SSH nếu có, hoặc Enter nếu chỉ dùng console: ' PBL4_SSH_PORT
```

```bash
if [ -n "$PBL4_SSH_PORT" ]; then
    ufw allow "${PBL4_SSH_PORT}/tcp"
fi
ufw default deny incoming
ufw default allow outgoing
for port in 80 443 9091 9092; do
    ufw allow in proto tcp from any to "$PBL4_IPV6" port "$port"
done
ufw enable
ufw status numbered
```

`deny incoming` từ chối kết nối đến theo mặc định. Bốn luật chỉ cho phép TCP tới đúng IPv6 đã chọn. Luật SSH theo số cổng cho phép quản trị qua các địa chỉ của máy. Có thể giới hạn thêm nguồn SSH tại security group hoặc router. Các cổng do Nginx trên host nhận trước khi chuyển tiếp vào Docker, nên firewall của Ubuntu kiểm soát kết nối vào listener Nginx. [Ubuntu UFW](https://documentation.ubuntu.com/server/how-to/security/firewalls/) · [Docker và UFW](https://docs.docker.com/engine/network/packet-filtering-firewalls/#docker-and-ufw)

Nếu là máy ở nhà, vào **IPv6 firewall** của router và cho phép TCP 80, 443, 9091, 9092 tới IPv6 của Ubuntu. Nếu là VPS, thêm bốn luật inbound IPv6 đó ở security group hoặc firewall nhà cung cấp. Cổng 80 cần truy cập được trong các lần xác minh và gia hạn. Giữ ICMPv6 thiết yếu cho hoạt động của IPv6. Cổng 8080, 9090, 5432 và 6379 dùng nội bộ.

## 5. Cấp chứng chỉ qua cổng 80

**Ubuntu Server.** Nginx sẽ phục vụ file xác minh trong `/var/www/remoteaccess-acme`. Site chỉ cần HTTP lúc này. Sau khi có chứng chỉ, HTTPS sẽ được bật ở bước sau.

```bash
. /root/remoteaccess-public.conf
install -d -m 0755 /var/www/remoteaccess-acme/.well-known/acme-challenge
cat > /etc/nginx/sites-available/remoteaccess-http <<EOF_HTTP
server {
    listen [::]:80;
    server_name ${PBL4_AUTHORITY};

    location ^~ /.well-known/acme-challenge/ {
        root /var/www/remoteaccess-acme;
        try_files \$uri =404;
    }
    location / {
        return 308 https://${PBL4_AUTHORITY}\$request_uri;
    }
}
EOF_HTTP
ln -sfn /etc/nginx/sites-available/remoteaccess-http /etc/nginx/sites-enabled/remoteaccess-http
nginx -t && systemctl reload nginx
printf 'remoteaccess-ipv6-ok\n' > /var/www/remoteaccess-acme/.well-known/acme-challenge/connectivity-check
ss -6ltnp 'sport = :80'
```

`nginx -t` kiểm tra cấu hình trước khi reload. `ln -sfn` kích hoạt site qua symlink. `ss -6` xác nhận socket IPv6 đang lắng nghe. Nginx trả file ACME trên HTTP. Các đường dẫn còn lại được chuyển sang HTTPS với mã 308.

**Máy ngoài có IPv6.** Điền đúng một trong hai ví dụ cho `PBL4_AUTHORITY`.

```bash
PBL4_AUTHORITY='remote.tenmiencuaban.vn'
# Nếu dùng IP trực tiếp, ví dụ định dạng: PBL4_AUTHORITY='[2001:db8::1234]'
curl -6fsS --connect-timeout 8 "http://$PBL4_AUTHORITY/.well-known/acme-challenge/connectivity-check"
```

Kết quả cần là `remoteaccess-ipv6-ok`. Nếu trả trang mặc định, kiểm tra hostname, IPv6 và `nginx -T`. Nếu timeout, kiểm tra firewall Ubuntu, router/security group và đường IPv6 của máy ngoài. Ở máy chỉ có IPv4, chọn một mạng khác có IPv6 để thử.

**Ubuntu Server.** Sau khi kiểm tra thành công, xin chứng chỉ. Certbot hỏi email quản trị và điều khoản ở lần đầu.

```bash
. /root/remoteaccess-public.conf
rm -f /var/www/remoteaccess-acme/.well-known/acme-challenge/connectivity-check
if [ -n "$PBL4_DNS" ]; then
    /snap/bin/certbot certonly --webroot -w /var/www/remoteaccess-acme \
        --cert-name remoteaccess -d "$PBL4_DNS"
else
    /snap/bin/certbot certonly --webroot -w /var/www/remoteaccess-acme \
        --cert-name remoteaccess --preferred-profile shortlived --ip-address "$PBL4_IPV6"
fi
/snap/bin/certbot certificates
```

`certonly` lấy PEM và lưu cấu hình gia hạn. `--webroot` đặt file xác minh trong thư mục Nginx đang phục vụ. `--cert-name remoteaccess` đặt đường dẫn ổn định `/etc/letsencrypt/live/remoteaccess/`. Với IP trực tiếp, `--ip-address` nhận IP trần và chứng chỉ mang chính IP đó trong SAN. Cần thấy `fullchain.pem` và `privkey.pem` tại đường dẫn trên trước khi bật TLS. [Xác minh HTTP-01 của Let’s Encrypt](https://letsencrypt.org/docs/challenge-types/#http-01-challenge)

## 6. Bật HTTPS, hai relay TLS và gia hạn

**Ubuntu Server.** Tạo cấu hình HTTPS ở Nginx host.

```bash
. /root/remoteaccess-public.conf
cat > /etc/nginx/sites-available/remoteaccess-https <<EOF_HTTPS
limit_req_zone \$binary_remote_addr zone=remoteaccess_ipv6_login:10m rate=10r/m;

server {
    listen [::]:443 ssl;
    server_name ${PBL4_AUTHORITY};
    ssl_certificate /etc/letsencrypt/live/remoteaccess/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/remoteaccess/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;

    client_max_body_size 1m;
    limit_req_status 429;
    proxy_http_version 1.1;
    proxy_set_header Host \$host;
    proxy_set_header X-Real-IP \$remote_addr;
    proxy_set_header X-Forwarded-For \$remote_addr;
    proxy_set_header X-Forwarded-Proto \$scheme;
    proxy_set_header Connection "";

    location = /api/v1/auth/register { return 403; }
    location ^~ /api/v1/auth/register/ { return 404; }
    location ^~ /internal/ { return 404; }

    location = /api/v1/auth/login {
        limit_req zone=remoteaccess_ipv6_login burst=10 nodelay;
        proxy_pass http://127.0.0.1:8080;
    }
    location = /api/v1/devices/events {
        proxy_buffering off;
        proxy_read_timeout 90s;
        proxy_pass http://127.0.0.1:8080;
    }
    location / {
        proxy_pass http://127.0.0.1:8080;
    }
}
EOF_HTTPS
ln -sfn /etc/nginx/sites-available/remoteaccess-https /etc/nginx/sites-enabled/remoteaccess-https
```

TLS kết thúc ở Nginx cho API. `proxy_pass` chuyển request sang Nginx Compose ở loopback. Các đường quản trị relay và đăng ký ADMIN công cộng bị chặn. `limit_req` giới hạn tốc độ đăng nhập theo địa chỉ client. `proxy_buffering off` đưa sự kiện SSE đến desktop ngay khi Nginx nhận được.

Tạo listener TCP cho relay. Nếu máy đã có một khối `stream` riêng, thêm hai `server` của ví dụ vào khối hiện có trước khi reload. Với cấu hình Nginx mới từ APT, dùng nguyên khối dưới đây.

```bash
cat > /etc/nginx/remoteaccess-stream.conf <<'EOF_STREAM'
stream {
    server {
        listen [::]:9091;
        proxy_connect_timeout 5s;
        proxy_timeout 1h;
        proxy_pass 127.0.0.1:9091;
    }
    server {
        listen [::]:9092;
        proxy_connect_timeout 5s;
        proxy_timeout 1h;
        proxy_pass 127.0.0.1:9092;
    }
}
EOF_STREAM
grep -qxF 'include /etc/nginx/remoteaccess-stream.conf;' /etc/nginx/nginx.conf ||
    printf '\ninclude /etc/nginx/remoteaccess-stream.conf;\n' >> /etc/nginx/nginx.conf
nginx -t && systemctl reload nginx
```

Hai listener IPv6 của Nginx chuyển byte TCP vào hai cổng IPv4 loopback. Relay Java nhận TLS nguyên vẹn và xuất trình chứng chỉ. `proxy_connect_timeout` giới hạn thời gian bắt kết nối tới relay local. `proxy_timeout` cho phiên rảnh tối đa một giờ. `include` nằm ở cấp chính của Nginx, ngoài khối `http`. [Nginx stream](https://nginx.org/en/docs/stream/ngx_stream_core_module.html)

Chuẩn bị khóa cho relay và deploy hook gia hạn.

```bash
install -d -m 0700 /etc/remoteaccess/relay-tls
install -d /etc/letsencrypt/renewal-hooks/deploy
cat > /etc/letsencrypt/renewal-hooks/deploy/50-remoteaccess <<'EOF_HOOK'
#!/bin/sh
set -eu
[ "${RENEWED_LINEAGE:-}" = /etc/letsencrypt/live/remoteaccess ] || exit 0

umask 077
dir=/etc/remoteaccess/relay-tls
openssl pkcs8 -topk8 -nocrypt -in "$RENEWED_LINEAGE/privkey.pem" -out "$dir/key.new"
chown 10001 "$dir/key.new"
chmod 0400 "$dir/key.new"
install -m 0444 "$RENEWED_LINEAGE/fullchain.pem" "$dir/fullchain.new"
mv -f "$dir/key.new" "$dir/key.pem"
mv -f "$dir/fullchain.new" "$dir/fullchain.pem"
nginx -t
systemctl reload nginx

if [ -x /usr/local/sbin/remoteaccess-compose ] &&
   [ -f /opt/remoteaccess/infrastructure/.env ]; then
    for relay in relay-proxy-1 relay-proxy-2; do
        if [ -n "$(/usr/local/sbin/remoteaccess-compose ps --quiet "$relay")" ]; then
            /usr/local/sbin/remoteaccess-compose up -d --no-deps --force-recreate "$relay"
        fi
    done
fi
EOF_HOOK
chmod 0700 /etc/letsencrypt/renewal-hooks/deploy/50-remoteaccess
RENEWED_LINEAGE=/etc/letsencrypt/live/remoteaccess /etc/letsencrypt/renewal-hooks/deploy/50-remoteaccess
systemctl list-timers --all snap.certbot.renew.timer
/snap/bin/certbot renew --dry-run
```

`openssl pkcs8` tạo định dạng khóa relay có thể đọc. UID 10001 là user của relay container, với quyền chỉ đọc khóa `0400`. Thư mục TLS chỉ root truy cập trên host. Hook cập nhật file, reload Nginx và tạo lại các relay đang chạy lần lượt để mỗi tiến trình đọc chứng chỉ mới. Phiên đi qua relay được tạo lại có thể gián đoạn. `renew --dry-run` thử đường xác minh với máy chủ staging. Bảo đảm timer của Certbot hiển thị lần chạy tiếp theo, đặc biệt nếu dùng chứng chỉ IP ngắn ngày. [Gia hạn và deploy hook của Certbot](https://eff-certbot.readthedocs.io/en/stable/using.html#renewing-certificates)

## 7. Tạo secret và khởi động dự án

**Ubuntu Server.** Khối này chỉ tạo `infrastructure/.env` lần đầu. Nếu file đã có, giữ các secret và cập nhật đúng hostname, bind, port và đường dẫn TLS trước khi chạy Compose.

```bash
cd /opt/remoteaccess
. /root/remoteaccess-public.conf
export PBL4_HOST
python3 - <<'PY'
import os
import secrets
from pathlib import Path

base = Path("infrastructure")
values = {
    "JWT_SECRET": secrets.token_hex(32),
    "RELAY_NODE_TOKEN": secrets.token_hex(32),
    "POSTGRES_PASSWORD": secrets.token_hex(32),
    "MANAGEMENT_PUBLIC_BIND": "127.0.0.1",
    "MANAGEMENT_PUBLIC_PORT": "8080",
    "RELAY_PUBLIC_BIND": "127.0.0.1",
}
for n in (1, 2):
    values[f"RELAY_{n}_ADVERTISED_HOST"] = os.environ["PBL4_HOST"]
    values[f"RELAY_{n}_TLS_CERT"] = "/etc/remoteaccess/relay-tls/fullchain.pem"
    values[f"RELAY_{n}_TLS_KEY"] = "/etc/remoteaccess/relay-tls/key.pem"
lines = []
for line in (base / ".env.vps.example").read_text().splitlines():
    key = line.partition("=")[0]
    if key in values:
        line = f"{key}={values.pop(key)}"
    lines.append(line)
lines.extend(f"{key}={value}" for key, value in values.items())
os.umask(0o077)
with (base / ".env").open("x") as output:
    output.write("\n".join(lines) + "\n")
PY
stat -c '%a %U %n' infrastructure/.env
git check-ignore infrastructure/.env
```

Ba giá trị ngẫu nhiên dùng cho chữ ký phiên đăng nhập, xác thực nội bộ relay và mật khẩu PostgreSQL. Relay công bố tên miền hoặc IPv6 trần cho Qt để app tìm đúng máy chủ. Các cổng Docker gắn với loopback. App nhận IPv6 công cộng thông qua Nginx host. Kỳ vọng `.env` có quyền `600` và được Git bỏ qua. Khi dùng lại data volume PostgreSQL, mật khẩu trong `.env` phải khớp mật khẩu đang lưu cho tài khoản DB.

Tạo lệnh dùng chung, sau đó build và chạy.

```bash
cat > /usr/local/sbin/remoteaccess-compose <<'EOF_COMPOSE'
#!/bin/sh
set -eu
cd /opt/remoteaccess
exec docker compose --env-file infrastructure/.env \
    -f infrastructure/docker-compose.yml -f infrastructure/docker-compose.tls.yml "$@"
EOF_COMPOSE
chmod 0755 /usr/local/sbin/remoteaccess-compose
remoteaccess-compose config --quiet &&
    remoteaccess-compose up -d --build --wait --wait-timeout 300
remoteaccess-compose ps
remoteaccess-compose exec -T redis redis-cli EXISTS relay:node:relay-proxy-1 relay:node:relay-proxy-2
curl -sS -o /dev/null -w 'Loopback API HTTP %{http_code}\n' http://127.0.0.1:8080/api/v1/devices
ss -ltnp
```

- `config --quiet` kiểm tra hai file Compose và biến môi trường mà không in secret. `up -d --build` build image từ đúng mã nguồn vừa checkout và chạy nền. Dockerfile đã có Java và Maven phục vụ build, vì vậy Ubuntu host chỉ cần Docker cho backend.
- `--wait` đợi service chạy hoặc healthy. `ps` phải hiện **7 container** gồm PostgreSQL, Redis, hai API, hai relay và một Nginx Compose. Hai API, PostgreSQL và Redis có healthcheck.
- `redis-cli EXISTS` trả **2** sau khi hai relay đã đăng ký. Nếu chỉ thấy 0 hoặc 1 ngay lúc khởi động, chờ vài giây rồi chạy lại. `curl` qua loopback phải trả **401** vì request vào đường dẫn bảo vệ mà chưa đưa token.
- `ss` cho thấy các socket `[::]:80`, `[::]:443`, `[::]:9091`, `[::]:9092` của Nginx và các cổng `127.0.0.1:8080`, `127.0.0.1:9091`, `127.0.0.1:9092` của Docker. Port 9090 của API Java, 5432 của PostgreSQL và 6379 của Redis chỉ ở trong stack này.

## 8. Kiểm tra từ Internet qua IPv6

**Máy ngoài.** Dùng Bash trên Linux, macOS có OpenSSL phù hợp hoặc WSL. Với IP trực tiếp, dùng có ngoặc vuông cho URL và điền IP trần riêng để xác minh SAN.

```bash
PBL4_AUTHORITY='remote.tenmiencuaban.vn'
PBL4_IP_LITERAL=''
# Nếu dùng IP trực tiếp, thay cả hai dòng trên theo định dạng:
# PBL4_AUTHORITY='[2001:db8::1234]'
# PBL4_IP_LITERAL='2001:db8::1234'

curl -6sS --connect-timeout 8 -o /dev/null -w 'HTTP redirect %{http_code}\n' \
    "http://$PBL4_AUTHORITY/"
for path in /api/v1/devices /internal/relay/nodes /api/v1/auth/register; do
    curl -6sS --connect-timeout 8 -o /dev/null -w "$path %{http_code}\n" \
        "https://$PBL4_AUTHORITY$path"
done

for port in 9091 9092; do
    printf '\nRelay TCP %s\n' "$port"
    if [ -n "$PBL4_IP_LITERAL" ]; then
        timeout 10 openssl s_client -6 -brief -connect "$PBL4_AUTHORITY:$port" \
            -noservername -verify_ip "$PBL4_IP_LITERAL" -verify_return_error </dev/null
    else
        timeout 10 openssl s_client -6 -brief -connect "$PBL4_AUTHORITY:$port" \
            -servername "$PBL4_AUTHORITY" -verify_hostname "$PBL4_AUTHORITY" \
            -verify_return_error </dev/null
    fi
done
```

Trên macOS, nếu hệ thống thiếu `timeout`, dùng WSL/Linux hoặc bỏ tiền tố `timeout 10` và nhấn Ctrl+C nếu lệnh chờ lâu. Kết quả mong đợi là HTTP **308**, API bảo vệ **401**, tuyến nội bộ **404**, đăng ký công cộng **403** và **Verification: OK** cho mỗi relay. `-6` buộc đi qua IPv6. `-verify_hostname` hoặc `-verify_ip` kiểm tra danh tính máy chủ. Thử kiểm tra trên mạng mà từng máy ADMIN và CHILD sẽ thực sự sử dụng.

Trên Windows cũng có thể thử riêng đường TCP từ PowerShell, nhập IPv6 trần thay cho địa chỉ mẫu.

```powershell
$ServerIPv6 = '2001:db8::1234'
80, 443, 9091, 9092 | ForEach-Object { Test-NetConnection -ComputerName $ServerIPv6 -Port $_ }
```

`TcpTestSucceeded : True` xác nhận cổng TCP tới được. Bài thử `curl` và OpenSSL ở trên kiểm tra thêm chứng chỉ TLS.

## 9. Tạo ADMIN và chạy hai máy desktop

**Ubuntu Server.** Nhập tên và mật khẩu ADMIN riêng cho lần cài này. Mật khẩu nhập ẩn và được truyền qua stdin, không đặt trực tiếp trong tham số `curl` hoặc lịch sử lệnh.

```bash
read -rp 'Tên ADMIN: ' admin_name
```

```bash
read -rsp 'Mật khẩu ADMIN: ' admin_pass && printf '\n'
```

```bash
printf '%s' "$admin_pass" | jq -Rs --arg user "$admin_name" '{username:$user,password:.}' |
    curl -sS -w '\nHTTP %{http_code}\n' -H 'Content-Type: application/json' \
        --data-binary @- http://127.0.0.1:8080/api/v1/auth/register
unset admin_name admin_pass
```

`jq` mã hóa ký tự trong mật khẩu thành chuỗi JSON đúng. `curl --data-binary @-` đọc JSON đó từ stdin và gửi POST. HTTP **201** nghĩa là tạo thành công. **409** nghĩa là tên đó đã có. **400** cần xem thông báo JSON để sửa dữ liệu nhập.

**Windows ADMIN và CHILD.** Dùng cùng phiên bản EXE. Nếu đã có cả bộ app, chuyển thẳng sang đoạn launcher. Nếu cần build, mở terminal Qt 6 MinGW 64 bit, chuẩn bị CMake, Ninja, Qt SVG và Linguist. Từ repo cùng commit đã ghi trên Ubuntu, dùng các lệnh CMD sau.

```bat
cmake -S clients/RemoteAccessApp -B build-ipv6-client -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-ipv6-client --target RemoteAccessApp
mkdir remoteaccess-client
copy build-ipv6-client\RemoteAccessApp.exe remoteaccess-client\
windeployqt --release --compiler-runtime remoteaccess-client\RemoteAccessApp.exe
```

`cmake -S ... -B ...` tạo cấu hình build. `cmake --build` biên dịch app. `windeployqt` tập hợp DLL Qt, plugin TLS và runtime vào thư mục EXE. Chép **cả thư mục** sang client còn lại, rồi thử launcher trên từng máy.

Lưu đoạn dưới thành `Open-RemoteAccess.cmd` cạnh EXE. Sửa URL thành tên miền thật hoặc URL IPv6 có ngoặc vuông. Các biến `REMOTE_…` chỉ áp dụng cho tiến trình app được mở qua launcher.

```bat
@echo off
setlocal
cd /d "%~dp0"
set "REMOTE_API_URL=https://remote.tenmiencuaban.vn"
rem Nếu dùng IP trực tiếp, ví dụ: set "REMOTE_API_URL=https://[2001:db8::1234]"
set "REMOTE_RELAY_MODE=dynamic"
set "REMOTE_RELAY_TLS=required"
set "REMOTE_ALLOW_HTTP="
set "REMOTE_API_CA_CERT="
set "REMOTE_RELAY_CA_CERT="
set "REMOTE_CA_FILE="
RemoteAccessApp.exe
```

**Ubuntu Desktop**, nếu client chạy Linux. Cài công cụ build trên máy có GUI, chọn phiên X11 đã thử nghiệm nếu cần điều khiển chuột và bàn phím.

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build qt6-base-dev qt6-svg-dev \
    qt6-tools-dev qt6-l10n-tools libx11-dev libxtst-dev
cmake -S clients/RemoteAccessApp -B build-ipv6-client -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-ipv6-client --target RemoteAccessApp

PBL4_AUTHORITY='remote.tenmiencuaban.vn'
env -u REMOTE_ALLOW_HTTP -u REMOTE_API_CA_CERT -u REMOTE_RELAY_CA_CERT -u REMOTE_CA_FILE \
    REMOTE_API_URL="https://$PBL4_AUTHORITY" REMOTE_RELAY_MODE=dynamic \
    REMOTE_RELAY_TLS=required ./build-ipv6-client/RemoteAccessApp
```

`REMOTE_API_URL` chọn địa chỉ HTTPS. `dynamic` yêu cầu API cấp relay online. `required` yêu cầu TLS. Việc xóa các biến CA cũ trong launcher giúp app dùng kho chứng chỉ hệ điều hành.

1. Đăng nhập ADMIN bằng tài khoản tạo trên Ubuntu.
2. Trong mục **Tài khoản**, tạo tài khoản CHILD và dùng đúng tên mà ứng dụng hiển thị.
3. Đăng nhập CHILD ở máy thứ hai. Bấm **Bật chia sẻ** trên máy CHILD.
4. Từ ADMIN, chọn thiết bị và kiểm tra hình ảnh, chuột, bàn phím trên một cửa sổ thử nghiệm. Bấm **Dừng chia sẻ** trên CHILD để kiểm tra phiên kết thúc.

## 10. Xem log, cập nhật và sao lưu

| Triệu chứng | Lệnh để xác định nguyên nhân |
|---|---|
| IPv6 timeout cả cổng 80 | Trên server dùng `ss -6ltnp 'sport = :80'` và `ufw status numbered`. Từ mạng ngoài dùng `dig +short AAAA TÊN_MIỀN` và thử lại file ACME. Kiểm tra địa chỉ đích và firewall router/VPS. |
| Certbot lỗi | Dùng `/snap/bin/certbot certificates`, `journalctl -u snap.certbot.renew.service -n 80 --no-pager` và xem `/var/log/letsencrypt/letsencrypt.log`. |
| HTTPS báo 502 | Dùng `curl -i http://127.0.0.1:8080/api/v1/devices`, `remoteaccess-compose ps`, `remoteaccess-compose logs --tail 80 nginx management-api-1 management-api-2`. |
| HTTPS được nhưng relay lỗi | Dùng hai lệnh OpenSSL ở mục 8, `remoteaccess-compose logs --tail 80 relay-proxy-1 relay-proxy-2` và `tail -n 80 /var/log/nginx/error.log`. Kiểm tra listener, TTL relay, file PEM và tên endpoint đã quảng bá. |
| Certbot gia hạn nhưng client thấy chứng chỉ cũ | Xem timer, log Certbot và ngày hiệu lực từ `openssl x509 -in /etc/remoteaccess/relay-tls/fullchain.pem -noout -dates`. Kiểm tra hook đã tạo lại relay và thử từ mạng ngoài. |
| Stack báo unhealthy | Dùng `remoteaccess-compose ps`, `remoteaccess-compose logs --tail 80 postgres redis management-api-1 management-api-2`, `free -h` và `df -h /`. Kiểm tra mật khẩu DB có tương ứng với volume đang dùng. |

**Sao lưu trên Ubuntu Server.** Ghi lại SHA, dump PostgreSQL và cấu hình cần để triển khai lại.

```bash
cd /opt/remoteaccess
backup="/root/remoteaccess-backups/$(date +%Y%m%d-%H%M%S)"
install -d -m 0700 "$backup"
umask 077
git rev-parse HEAD > "$backup/commit"
install -m 0600 infrastructure/.env "$backup/compose.env"
install -m 0600 /root/remoteaccess-public.conf "$backup/public.conf"
remoteaccess-compose exec -T postgres sh -c 'pg_dump -Fc -U "$POSTGRES_USER" "$POSTGRES_DB"' \
    > "$backup/postgres.dump" &&
    remoteaccess-compose exec -T postgres pg_restore --list < "$backup/postgres.dump" > /dev/null
```

`pg_dump -Fc` xuất PostgreSQL dạng custom. `pg_restore --list` kiểm tra đọc được danh mục trong bản dump. Lưu một bản mã hóa ngoài máy, kèm cấu hình Nginx, các luật firewall và hook gia hạn.

Để thử khôi phục, nhập đường dẫn thư mục bản sao lưu, rồi tạo một database mới trên cùng PostgreSQL. Chạy khi server có đủ dung lượng trống.

```bash
read -erp 'Thư mục backup trong /root/remoteaccess-backups: ' backup
```

```bash
restore_db="remote_restore_$(date +%Y%m%d_%H%M%S)"
test -s "$backup/postgres.dump" &&
    remoteaccess-compose exec -T postgres sh -c 'createdb -U "$POSTGRES_USER" "$1"' sh "$restore_db" &&
    remoteaccess-compose exec -T postgres sh -c 'pg_restore --exit-on-error -U "$POSTGRES_USER" -d "$1"' \
        sh "$restore_db" < "$backup/postgres.dump" &&
    remoteaccess-compose exec -T postgres sh -c 'psql -U "$POSTGRES_USER" -d "$1" -c "\dt"' sh "$restore_db"
```

`createdb` tạo database thử riêng. `pg_restore --exit-on-error` dừng khi gặp lỗi và `psql … \dt` liệt kê các bảng đã phục hồi. Chọn thời gian bảo trì riêng nếu cần chuyển ứng dụng sang bản dữ liệu đã phục hồi.

**Cập nhật Ubuntu Server sau khi PR được merge.** Dùng cửa sổ bảo trì. Sao lưu thành công trước rồi mới lấy phiên bản mới.

```bash
cd /opt/remoteaccess
git status --short
git fetch origin
git log --oneline HEAD..origin/main
git switch --detach origin/main &&
    remoteaccess-compose config --quiet &&
    remoteaccess-compose up -d --build --wait --wait-timeout 300
remoteaccess-compose ps
```

`git fetch` cập nhật các tham chiếu từ GitHub. `git log` cho biết thay đổi sắp triển khai. Chuỗi `&&` ngừng ở bước gặp lỗi. Sau khi kiểm chứng mục 8 và 9, ghi SHA mới bằng `git rev-parse HEAD | tee /root/remoteaccess-source-commit`. Khi quay lại phiên bản cũ, kiểm tra tính tương thích schema PostgreSQL hoặc dùng backup của phiên bản cũ vì Hibernate có thể đã cập nhật schema.

Để dừng stack và giữ volume, dùng `remoteaccess-compose stop`. Để chạy lại với cùng dữ liệu, dùng `remoteaccess-compose up -d --wait`. Tùy chọn `down -v` xóa cả volume dữ liệu.

Sau khi reboot Ubuntu, kiểm tra `systemctl is-active docker nginx`, `remoteaccess-compose ps` và `systemctl list-timers --all snap.certbot.renew.timer`. Thử lại mục 8 từ máy ngoài để xác nhận các listener và chứng chỉ vẫn hoạt động.
