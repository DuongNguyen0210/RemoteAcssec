#include "devicecardwidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QIcon>
#include <QPixmap>
#include <QFile>
#include <QGraphicsDropShadowEffect>

DeviceCardWidget::DeviceCardWidget(const QString& name, const QString& OS, const QString& ip, const QString& status, const QString& uptime, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("deviceCard");
    this->setAttribute(Qt::WA_StyledBackground, true);
    this->setAttribute(Qt::WA_Hover, true);
    this->setFixedSize(240, 300);

    QFile styleFile(":/styles/Resources/styles/devicecard.qss");

    if (styleFile.open(QFile::ReadOnly))
    {
        QString styleSheet = QLatin1String(styleFile.readAll());
        this->setStyleSheet(styleSheet);
        styleFile.close();
    }

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(0);
    shadow->setOffset(0, 0);
    shadow->setColor(QColor(37, 99, 235, 0));
    this->setGraphicsEffect(shadow);

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
    QLabel *lblName = new QLabel(QString("<b>%1</b>").arg(name), this);
    lblName->setObjectName("deviceName");
    QLabel *lblOS = new QLabel(QString(OS), this);
    lblOS->setProperty("role", "muted");
    NameLayout->addWidget(lblName);
    NameLayout->addWidget(lblOS);

    QPushButton *bttRemove = new QPushButton(this);
    bttRemove->setObjectName("removeDeviceButton");
    bttRemove->setCursor(Qt::PointingHandCursor);
    bttRemove->setFixedHeight(25);
    bttRemove->setFixedWidth(25);
    bttRemove->setIcon(QIcon(":/icons/Resources/icons/x.svg"));


    HeaderLayout->addWidget(lblIcon);
    HeaderLayout->addLayout(NameLayout);
    HeaderLayout->addWidget(bttRemove);

    layout->addLayout(HeaderLayout);

    layout->addStretch();

    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(6);

    QHBoxLayout *lblStatusLayout = new QHBoxLayout();
    lblStatusLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *lblStatus = new QLabel("Status");
    lblStatus->setProperty("role", "metaLabel");
    QLabel *lblStatusInfo = new QLabel(status);
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
    QLabel *lblIp = new QLabel("IP Address");
    lblIp->setProperty("role", "metaLabel");
    QLabel *lblIpInfo = new QLabel(ip);
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
    QLabel *lblUptime = new QLabel("Uptime");
    lblUptime->setProperty("role", "metaLabel");

    QLabel *lblUptimeInfo = new QLabel(uptime);
    lblUptimeInfo->setProperty("role", "strongValue");

    lblUptimeLayout->addWidget(lblUptime);
    lblUptimeLayout->addStretch();
    lblUptimeLayout->addWidget(lblUptimeInfo);
    infoLayout->addLayout(lblUptimeLayout);

    layout->addLayout(infoLayout);

    QPushButton *btnConnect = new QPushButton("Connect", this);
    btnConnect->setObjectName("connectButton");
    btnConnect->setCursor(Qt::PointingHandCursor);

    layout->addStretch();
    layout->addWidget(btnConnect);
}
