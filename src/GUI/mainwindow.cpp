#include "mainwindow.h"
#include "Pages/devicespage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName("mainWindow");
    resize(1200, 800);
    setWindowTitle("Remote Access App");
    setupUi();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("appRoot");
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    sidebar = new SidebarWidget(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(250);

    topbar = new TopBarWidget(this);
    topbar->setObjectName("topbar");
    topbar->setFixedHeight(100);

    QWidget *rightContentWidget = new QWidget(this);
    rightContentWidget->setObjectName("contentShell");
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContentWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    stackedWidget = new QStackedWidget(this);
    stackedWidget->setObjectName("contentStack");

    DevicesPage *pageDevices = new DevicesPage(this);
    QLabel *pageSessions = new QLabel("ĐÂY LÀ TRANG PHIÊN KẾT NỐI (SESSIONS)", this);
    QLabel *pageSettings = new QLabel("ĐÂY LÀ TRANG CÀI ĐẶT (SETTINGS)", this);
    QLabel *pageLogs = new QLabel("ĐÂY LÀ TRANG NHẬT KÝ (LOGS)", this);

    pageSessions->setAlignment(Qt::AlignCenter);
    pageSettings->setAlignment(Qt::AlignCenter);
    pageLogs->setAlignment(Qt::AlignCenter);

    stackedWidget->addWidget(pageDevices);
    stackedWidget->addWidget(pageSessions);
    stackedWidget->addWidget(pageSettings);
    stackedWidget->addWidget(pageLogs);

    rightLayout->addWidget(topbar);
    rightLayout->addWidget(stackedWidget);


    mainLayout->addWidget(sidebar);
    mainLayout->addWidget(rightContentWidget);

    connect(sidebar, &SidebarWidget::pageChanged, stackedWidget, &QStackedWidget::setCurrentIndex);
}
