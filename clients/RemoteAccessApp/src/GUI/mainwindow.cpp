#include "mainwindow.h"
#include "Pages/devicespage.h"
#include "Pages/logspage.h"
#include "Pages/sessionspage.h"
#include "Pages/settingspage.h"


#include <QHBoxLayout>
#include <QStringList>
#include <QVBoxLayout>

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
    centralWidget->setAttribute(Qt::WA_StyledBackground, true);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    sidebar = new SidebarWidget(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(256);

    topbar = new TopBarWidget(this);
    topbar->setObjectName("topbar");
    topbar->setFixedHeight(64);

    QWidget *rightContentWidget = new QWidget(this);
    rightContentWidget->setObjectName("contentShell");
    rightContentWidget->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContentWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    stackedWidget = new QStackedWidget(this);
    stackedWidget->setObjectName("contentStack");

    DevicesPage *pageDevices = new DevicesPage(this);
    SessionsPage *pageSessions = new SessionsPage(this);
    SettingsPage *pageSettings = new SettingsPage(this);
    LogsPage *pageLogs = new LogsPage(this);

    stackedWidget->addWidget(pageDevices);
    stackedWidget->addWidget(pageSessions);
    stackedWidget->addWidget(pageSettings);
    stackedWidget->addWidget(pageLogs);

    rightLayout->addWidget(topbar);
    rightLayout->addWidget(stackedWidget);

    mainLayout->addWidget(sidebar);
    mainLayout->addWidget(rightContentWidget);

    connect(sidebar, &SidebarWidget::pageChanged, stackedWidget, &QStackedWidget::setCurrentIndex);
    connect(sidebar, &SidebarWidget::pageChanged, this, [this](int pageIndex) {
        const QStringList placeholders = {
            "Search devices...",
            "Search sessions...",
            "Search settings...",
            "Search logs..."
        };
        if (pageIndex >= 0 && pageIndex < placeholders.size()) {
            topbar->setSearchPlaceholder(placeholders.at(pageIndex));
        }
    });
}
