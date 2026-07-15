#include "devicecardwidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QIcon>
#include <QPixmap>
#include <QFile>

DeviceCardWidget::DeviceCardWidget(const QString& name, const QString& OS, const QString& ip, const QString& status, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("deviceCard");
    this->setFixedSize(250, 180);

    QFile styleFile(":/styles/Resources/styles/devicecard.qss");

    if (styleFile.open(QFile::ReadOnly))
    {
        QString styleSheet = QLatin1String(styleFile.readAll());
        this->setStyleSheet(styleSheet);
        styleFile.close();
    }

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *HeaderLayout = new QHBoxLayout();

    QLabel *lblIcon = new QLabel(this);
    lblIcon->setObjectName("deviceIcon");
    QPixmap avatarPixmap(":/icons/Resources/icons/computer.svg");
    QPixmap Icon = avatarPixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    lblIcon->setPixmap(Icon);

    QVBoxLayout *NameLayout = new QVBoxLayout();
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

    QVBoxLayout *infoLayout = new QVBoxLayout();

    QHBoxLayout *lblStatusLayout = new QHBoxLayout();
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

    infoLayout->addWidget(line1);

    QHBoxLayout *lblIpLayout = new QHBoxLayout();
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

    infoLayout->addWidget(line2);

    layout->addLayout(infoLayout);

    QPushButton *btnConnect = new QPushButton("Connect", this);
    btnConnect->setObjectName("connectButton");
    btnConnect->setCursor(Qt::PointingHandCursor);

    layout->addStretch();
    layout->addWidget(btnConnect);
}
