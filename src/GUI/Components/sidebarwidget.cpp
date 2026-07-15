#include "sidebarwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QButtonGroup>
#include <QIcon>
#include <QStyle>
#include <QFile>

SidebarWidget::SidebarWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void SidebarWidget::setupUi()
{

    setObjectName("sidebar");
    this->setAttribute(Qt::WA_StyledBackground, true);
    this->setAttribute(Qt::WA_Hover, true);

    QFile styleFile(":/styles/Resources/styles/sidebar.qss");

    if (styleFile.open(QFile::ReadOnly))
    {
        QString styleSheet = QLatin1String(styleFile.readAll());
        this->setStyleSheet(styleSheet);
        styleFile.close();
    }

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QHBoxLayout *LogoLayout = new QHBoxLayout();
    LogoLayout->addStretch();
    QLabel *logoLabel = new QLabel("<b>RemoteAccess</b>", this);
    logoLabel->setObjectName("lblName");
    logoLabel->setFixedHeight(100);
    LogoLayout->addWidget(logoLabel);
    LogoLayout->addStretch();
    layout->addLayout(LogoLayout);

    QFrame *lineTop = new QFrame(this);
    lineTop->setFrameShape(QFrame::HLine);
    lineTop->setObjectName("separatorLine");
    layout->addWidget(lineTop);

    QVBoxLayout *MenuLayout = new QVBoxLayout();
    MenuLayout->setContentsMargins(8, 10, 8, 10);
    MenuLayout->setSpacing(4);

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

    layout->addLayout(MenuLayout);

    QFrame *lineBottom = new QFrame(this);
    lineBottom->setFrameShape(QFrame::HLine);
    lineBottom->setObjectName("separatorLine");
    layout->addWidget(lineBottom);

    QVBoxLayout *ConnLayout = new QVBoxLayout();
    ConnLayout->setContentsMargins(8, 10, 8, 0);
    QPushButton *btnNewConn = new QPushButton("Add New Connection", this);
    btnNewConn->setObjectName("newConnButton");
    btnNewConn->setCursor(Qt::PointingHandCursor);
    ConnLayout->addWidget(btnNewConn);
    layout->addLayout(ConnLayout);

    layout->addStretch();

    btnDevices->setChecked(true);
}
