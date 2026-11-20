#include "MainWindow.h"
#include "GUI/Pages/DevicesPage.h"
#include "GUI/Pages/LogsPage.h"
#include "GUI/Pages/SessionsPage.h"
#include "GUI/Pages/SettingsPage.h"
#include "GUI/Pages/AccountPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

MainWindow::MainWindow(DevicesPage *devicesPage, AccountPage *accountPage, SettingsPage *settingsPage, QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName("mainWindow");
    resize(1200, 800);
    setWindowTitle("RemoteAccess");
    setupUi(devicesPage, accountPage, settingsPage);
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi(DevicesPage *devicesPage, AccountPage *accountPage, SettingsPage *settingsPage)
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

    pageSessions = new SessionsPage(this);
    connect(pageSessions, &SessionsPage::disconnectRequested, this, &MainWindow::disconnectRequested);
    SettingsPage *pageSettings = settingsPage ? settingsPage : new SettingsPage(this);
    pageLogs = new LogsPage(this);

    if (devicesPage) {
        stackedWidget->addWidget(devicesPage);
    }
    stackedWidget->addWidget(pageSessions);
    stackedWidget->addWidget(pageSettings);
    stackedWidget->addWidget(pageLogs);
    if (accountPage) {
        stackedWidget->addWidget(accountPage);
        stackedWidget->setCurrentWidget(accountPage);
    }

    if (devicesPage) {
        connect(topbar, &TopBarWidget::searchChanged, this,
                [this, devicesPage](const QString &query) {
                    devicesPage->setSearchQuery(query);
                    if (!query.trimmed().isEmpty() && stackedWidget->currentIndex() != 0)
                        sidebar->selectPage(0);
                });
        connect(devicesPage, &DevicesPage::clearSearchRequested,
                topbar, &TopBarWidget::clearSearch);
    }

    topbar->setVisible(stackedWidget->currentIndex() == 0);
    rightLayout->addWidget(topbar);
    rightLayout->addWidget(stackedWidget);

    mainLayout->addWidget(sidebar);
    mainLayout->addWidget(rightContentWidget);

    connect(sidebar, &SidebarWidget::pageChanged, this, [this](int pageIndex) {
        stackedWidget->setCurrentIndex(pageIndex);
        topbar->setVisible(pageIndex == 0);
        emit pageSelected(pageIndex);
    });
}

void MainWindow::showRemoteSession()
{
    pageSessions->showSession();
    sidebar->selectPage(1);
}

void MainWindow::setRemoteFrame(const QImage &image) { pageSessions->setFrame(image); }
void MainWindow::clearRemoteFrame() { pageSessions->clearFrame(); }

void MainWindow::addSessionLog(const QString &event, const QString &detail, bool warning, quint64 sessionId)
{
    pageLogs->addEntry(event, detail, warning, sessionId);
}
