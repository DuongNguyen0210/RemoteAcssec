#include "sidebarwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QButtonGroup>
#include <QIcon>
#include <QStyle>

SidebarWidget::SidebarWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void SidebarWidget::setupUi()
{

    setObjectName("sidebar");
    this->setAttribute(Qt::WA_StyledBackground, true);
    this->setAttribute(Qt::WA_Hover, true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget *logoContainer = new QWidget(this);
    logoContainer->setObjectName("logoContainer");
    logoContainer->setAttribute(Qt::WA_StyledBackground, true);

    QHBoxLayout *LogoLayout = new QHBoxLayout(logoContainer);
    LogoLayout->setObjectName("layoutName");
    LogoLayout->setContentsMargins(16, 20, 16, 20);
    LogoLayout->setSpacing(12);
    QLabel *avatarLabel = new QLabel("RA", this);
    avatarLabel->setObjectName("brandAvatar");
    avatarLabel->setFixedSize(40, 40);
    avatarLabel->setAlignment(Qt::AlignCenter);
    QLabel *logoLabel = new QLabel("<b>RemoteAccess</b>", this);
    logoLabel->setObjectName("lblName");
    QLabel *roleLabel = new QLabel("IT Administrator", this);
    roleLabel->setObjectName("lblRole");

    QVBoxLayout *brandTextLayout = new QVBoxLayout();
    brandTextLayout->setContentsMargins(0, 0, 0, 0);
    brandTextLayout->setSpacing(2);
    brandTextLayout->addWidget(logoLabel);
    brandTextLayout->addWidget(roleLabel);

    LogoLayout->addWidget(avatarLabel);
    LogoLayout->addLayout(brandTextLayout);
    LogoLayout->addStretch();

    layout->addWidget(logoContainer);

    QVBoxLayout *ConnLayout = new QVBoxLayout();
    ConnLayout->setContentsMargins(16, 16, 16, 16);
    QPushButton *btnNewConn = new QPushButton("New Connection", this);
    btnNewConn->setIcon(QIcon(":/icons/Resources/icons/add-fill.svg"));
    btnNewConn->setObjectName("newConnButton");
    btnNewConn->setCursor(Qt::PointingHandCursor);
    btnNewConn->setFixedHeight(44);
    ConnLayout->addWidget(btnNewConn);
    layout->addLayout(ConnLayout);

    QVBoxLayout *MenuLayout = new QVBoxLayout();
    MenuLayout->setContentsMargins(16, 0, 16, 16);
    MenuLayout->setSpacing(8);

    QButtonGroup *menuGroup = new QButtonGroup(this);
    menuGroup->setExclusive(true);

    auto createMenuItem = [this, MenuLayout, menuGroup](const QString &text, const QString &iconPath, int pageIndex) -> QPushButton*
    {
        QWidget *row = new QWidget(this);
        row->setObjectName("menuRow");

        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(0);

        QPushButton *btn = new QPushButton(text, row);
        btn->setObjectName("menuButton");
        btn->setIcon(QIcon(iconPath));
        btn->setIconSize(QSize(20, 20));
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(44);

        QFrame *indicator = new QFrame(row);
        indicator->setObjectName("activeIndicator");
        indicator->setFixedWidth(4);
        indicator->setProperty("active", false);

        rowLayout->addWidget(btn, 1);
        rowLayout->addWidget(indicator);

        menuGroup->addButton(btn);
        MenuLayout->addWidget(row);

        connect(btn, &QPushButton::toggled, this, [this, indicator, pageIndex](bool checked){
            indicator->setProperty("active", checked);
            indicator->style()->unpolish(indicator);
            indicator->style()->polish(indicator);
            if (checked)
                emit pageChanged(pageIndex);
        });

        return btn;
    };

    QPushButton *btnDevices  = createMenuItem("Devices",  ":/icons/Resources/icons/devices.svg",  0);
    createMenuItem("Sessions", ":/icons/Resources/icons/sessions.svg", 1);
    createMenuItem("Settings", ":/icons/Resources/icons/settings.svg", 2);
    createMenuItem("Logs",     ":/icons/Resources/icons/logs.svg",     3);
    createMenuItem("Accounts", ":/icons/Resources/icons/circle-user.svg", 4);

    layout->addLayout(MenuLayout);

    layout->addStretch();

    btnDevices->setChecked(true);
}
