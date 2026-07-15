#include "devicecardwidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QPixmap>

DeviceCardWidget::DeviceCardWidget(const QString& name, const QString& OS, const QString& ip, const QString& status, QWidget *parent)
    : QWidget(parent)
{
    this->setFixedSize(250, 180);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *HeaderLayout = new QHBoxLayout();

    QLabel *lblIcon = new QLabel(this);
    QPixmap avatarPixmap(":/icons/Resources/icons/computer.png");
    QPixmap Icon = avatarPixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    lblIcon->setPixmap(Icon);

    QVBoxLayout *NameLayout = new QVBoxLayout();
    QLabel *lblName = new QLabel(QString("<b>%1</b>").arg(name), this);
    QLabel *lblOS = new QLabel(QString(OS), this);
    NameLayout->addWidget(lblName);
    NameLayout->addWidget(lblOS);

    HeaderLayout ->addWidget(lblIcon);
    HeaderLayout ->addLayout(NameLayout);

    layout->addLayout(HeaderLayout);

    QVBoxLayout *infoLayout = new QVBoxLayout();

    QHBoxLayout *lblStatusLayout = new QHBoxLayout();
    QLabel *lblStatus = new QLabel("Status");
    QLabel *lblStatusInfo = new QLabel(status);
    lblStatusLayout->addWidget(lblStatus);
    lblStatusLayout->addStretch();
    lblStatusLayout->addWidget(lblStatusInfo);

    infoLayout->addLayout(lblStatusLayout);

    QFrame *line1 = new QFrame(this);
    line1->setFrameShape(QFrame::HLine);

    infoLayout->addWidget(line1);

    QHBoxLayout *lblIpLayout = new QHBoxLayout();
    QLabel *lblIp = new QLabel("IP Address");
    QLabel *lblIpInfo = new QLabel(ip);
    lblIpLayout->addWidget(lblIp);
    lblIpLayout->addStretch();
    lblIpLayout->addWidget(lblIpInfo);

    infoLayout->addLayout(lblIpLayout);

    QFrame *line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);

    infoLayout->addWidget(line2);

    layout->addLayout(infoLayout);

    QPushButton *btnConnect = new QPushButton("Connect", this);

    layout->addStretch();
    layout->addWidget(btnConnect);
}
