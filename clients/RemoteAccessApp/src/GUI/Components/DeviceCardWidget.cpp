#include "DeviceCardWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QIcon>
#include <QPixmap>
#include <QTimer>

DeviceCardWidget::DeviceCardWidget(const DeviceInfo &info, QWidget *parent)
    : DeviceCardWidget(
        info.sessionId,
        info.deviceName,
        info.os,
        info.ipAddress,
        QStringLiteral("Online"),
        info.username,
        parent)
{
    m_telemetry = info.telemetry;
    m_telemetryElapsed.restart();
    refreshTelemetry();
}

DeviceCardWidget::DeviceCardWidget(const QString &agentSessionId, const QString &name,
                                   const QString &OS, const QString &ip,
                                   const QString &status, const QString &account,
                                   QWidget *parent)
    : QWidget(parent)
    , m_agentSessionId(agentSessionId)
{
    setObjectName("deviceCard");
    this->setAttribute(Qt::WA_StyledBackground, true);
    this->setAttribute(Qt::WA_Hover, true);
    this->setMinimumSize(360, 260);
    this->setMaximumWidth(520);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QHBoxLayout *HeaderLayout = new QHBoxLayout();
    HeaderLayout->setSpacing(10);
    QLabel *lblIcon = new QLabel(this);
    lblIcon->setFixedSize(48, 48);
    lblIcon->setObjectName("deviceIcon");
    QPixmap avatarPixmap(":/icons/Resources/icons/computer.svg");
    lblIcon->setPixmap(avatarPixmap);
    lblIcon->setAlignment(Qt::AlignCenter);

    QVBoxLayout *NameLayout = new QVBoxLayout();
    NameLayout->setSpacing(2);
    NameLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *lblName = new QLabel(name, this);
    lblName->setTextFormat(Qt::PlainText);
    lblName->setObjectName("deviceName");
    QLabel *lblOS = new QLabel(QString(OS), this);
    lblOS->setTextFormat(Qt::PlainText);
    lblOS->setProperty("role", "muted");
    NameLayout->addWidget(lblName);
    NameLayout->addWidget(lblOS);

    HeaderLayout->addWidget(lblIcon);
    HeaderLayout->addLayout(NameLayout);

    layout->addLayout(HeaderLayout);

    layout->addStretch();

    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(6);

    QHBoxLayout *lblStatusLayout = new QHBoxLayout();
    lblStatusLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *lblStatus = new QLabel("Trạng thái");
    lblStatus->setProperty("role", "metaLabel");
    QLabel *lblStatusInfo = new QLabel(status.compare("Online", Qt::CaseInsensitive) == 0 ? QStringLiteral("Trực tuyến")
        : status.compare("Offline", Qt::CaseInsensitive) == 0 ? QStringLiteral("Ngoại tuyến") : status);
    lblStatusInfo->setObjectName("statusChip");
    QString normalizedStatus = status.toLower().replace(" ", "");
    lblStatusInfo->setProperty("status", normalizedStatus);
    lblStatusLayout->addWidget(lblStatus);
    lblStatusLayout->addStretch();
    lblStatusLayout->addWidget(lblStatusInfo);
    infoLayout->addLayout(lblStatusLayout);

    QFrame *line1 = new QFrame(this);
    line1->setFrameShape(QFrame::HLine);
    line1->setObjectName("separatorLine");
    infoLayout->addWidget(line1);

    QHBoxLayout *lblIpLayout = new QHBoxLayout();
    lblIpLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *lblIp = new QLabel("Địa chỉ IP");
    lblIp->setProperty("role", "metaLabel");
    QLabel *lblIpInfo = new QLabel(ip);
    lblIpInfo->setTextFormat(Qt::PlainText);
    lblIpInfo->setProperty("role", "strongValue");
    lblIpLayout->addWidget(lblIp);
    lblIpLayout->addStretch();
    lblIpLayout->addWidget(lblIpInfo);
    infoLayout->addLayout(lblIpLayout);

    QFrame *line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setObjectName("separatorLine");
    infoLayout->addWidget(line2);

    QHBoxLayout *lblUptimeLayout = new QHBoxLayout();
    lblUptimeLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *lblUptime = new QLabel("Tài khoản");
    lblUptime->setProperty("role", "metaLabel");

    QLabel *lblUptimeInfo = new QLabel(account);
    lblUptimeInfo->setTextFormat(Qt::PlainText);
    lblUptimeInfo->setProperty("role", "strongValue");

    lblUptimeLayout->addWidget(lblUptime);
    lblUptimeLayout->addStretch();
    lblUptimeLayout->addWidget(lblUptimeInfo);
    infoLayout->addLayout(lblUptimeLayout);

    layout->addLayout(infoLayout);

    const auto metric = [this, layout](const QString &caption, const QString &objectName) {
        auto *row = new QHBoxLayout;
        auto *name = new QLabel(caption, this);
        name->setProperty("role", "metaLabel");
        auto *value = new QLabel(QStringLiteral("—"), this);
        value->setObjectName(objectName);
        value->setTextFormat(Qt::PlainText);
        value->setProperty("role", "strongValue");
        row->addWidget(name); row->addStretch(); row->addWidget(value);
        layout->addLayout(row);
        return value;
    };
    m_cpuValue = metric(QStringLiteral("CPU"), QStringLiteral("telemetryCpu"));
    m_memoryValue = metric(QStringLiteral("RAM"), QStringLiteral("telemetryMemory"));
    m_rxValue = metric(QStringLiteral("Mạng ↓ Nhận"), QStringLiteral("telemetryRx"));
    m_txValue = metric(QStringLiteral("Mạng ↑ Gửi"), QStringLiteral("telemetryTx"));
    m_telemetryState = new QLabel(this);
    m_telemetryState->setObjectName(QStringLiteral("telemetryState"));
    m_telemetryState->setProperty("role", "metaLabel");
    m_telemetryState->setWordWrap(true);
    m_telemetryState->setToolTip(QStringLiteral("Số liệu của máy CHILD. Mạng tính theo byte/giây, ưu tiên card mạng vật lý đang hoạt động; không tính loopback. Không phải kiểm tra tốc độ Internet."));
    layout->addWidget(m_telemetryState);
    m_telemetryElapsed.start();
    auto *ageTimer = new QTimer(this);
    ageTimer->setInterval(1000);
    connect(ageTimer, &QTimer::timeout, this, &DeviceCardWidget::refreshTelemetry);
    ageTimer->start();
    refreshTelemetry();

    m_connectButton = new QPushButton("Kết nối", this);
    m_connectButton->setObjectName("connectButton");
    m_connectButton->setProperty("role", "primaryActionButton");
    m_connectButton->setCursor(Qt::PointingHandCursor);
    m_connectButton->setFixedHeight(40);
    connect(m_connectButton, &QPushButton::clicked, this, [this]() {
        emit connectRequested(m_agentSessionId);
    });

    layout->addStretch();
    layout->addWidget(m_connectButton);
}

void DeviceCardWidget::setPendingConnection(const QString &targetSessionId)
{
    const bool pending = !targetSessionId.isEmpty();
    m_connectButton->setEnabled(!pending);
    m_connectButton->setText(pending && m_agentSessionId == targetSessionId
        ? QStringLiteral("Đang kết nối…") : QStringLiteral("Kết nối"));
}

void DeviceCardWidget::refreshTelemetry()
{
    const auto missing = QStringLiteral("—");
    m_cpuValue->setText(missing); m_memoryValue->setText(missing);
    m_rxValue->setText(missing); m_txValue->setText(missing);
    if (!m_telemetry) {
        m_telemetryState->setText(QStringLiteral("Chưa có số liệu"));
        return;
    }
    const auto &s = *m_telemetry;
    if (s.ageMs + m_telemetryElapsed.elapsed() >= TelemetryInfo::STALE_AFTER_MS) {
        m_telemetryState->setText(QStringLiteral("Mất cập nhật số liệu"));
        return;
    }
    if (s.cpuPercent) m_cpuValue->setText(QString::number(*s.cpuPercent, 'f', 1) + QStringLiteral(" %"));
    if (s.memoryUsedBytes && s.memoryTotalBytes) {
        constexpr double GiB = 1024.0 * 1024 * 1024;
        m_memoryValue->setText(QStringLiteral("%1 / %2 GiB (%3 %)")
                .arg(double(*s.memoryUsedBytes) / GiB, 0, 'f', 1)
                .arg(double(*s.memoryTotalBytes) / GiB, 0, 'f', 1)
                .arg(100.0 * double(*s.memoryUsedBytes) / double(*s.memoryTotalBytes), 0, 'f', 1));
    }
    const auto rate = [](double bytes) {
        const char *units[] = {"B/s", "KiB/s", "MiB/s", "GiB/s"};
        int unit = 0;
        while (bytes >= 1024 && unit < 3) { bytes /= 1024; ++unit; }
        return QString::number(bytes, 'f', 1) + ' ' + QLatin1String(units[unit]);
    };
    if (s.networkRxBytesPerSecond) m_rxValue->setText(rate(*s.networkRxBytesPerSecond));
    if (s.networkTxBytesPerSecond) m_txValue->setText(rate(*s.networkTxBytesPerSecond));
    const bool complete = s.cpuPercent && s.memoryTotalBytes && s.networkRxBytesPerSecond;
    m_telemetryState->setText(complete ? QStringLiteral("Đang cập nhật số liệu") : QStringLiteral("Đang lấy mẫu / một số chỉ số chưa có"));
}
