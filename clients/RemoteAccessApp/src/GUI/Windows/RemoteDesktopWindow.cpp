#include "RemoteDesktopWindow.h"
#include "../Components/RemoteScreenWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QIcon>

RemoteDesktopWindow::RemoteDesktopWindow(quint64 sessionId, const QString &targetName, QWidget *parent)
    : QMainWindow(parent)
    , m_sessionId(sessionId)
    , m_targetName(targetName)
{
    setupUi();
}

void RemoteDesktopWindow::setupUi()
{
    setObjectName("remoteDesktopWindow");
    resize(1280, 768);
    setMinimumSize(800, 500);

    setWindowTitle(QStringLiteral("Điều khiển từ xa - %1 (Phiên: #%2)")
                       .arg(m_targetName.isEmpty() ? QStringLiteral("Máy con") : m_targetName)
                       .arg(m_sessionId));

    auto *central = new QWidget(this);
    central->setObjectName("remoteDesktopRoot");
    central->setStyleSheet(QStringLiteral("background-color: #121416;"));
    setCentralWidget(central);

    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Header toolbar
    auto *headerWidget = new QWidget(central);
    headerWidget->setObjectName("remoteHeaderBar");
    headerWidget->setFixedHeight(52);
    headerWidget->setStyleSheet(QStringLiteral(
        "background-color: #1c1f22; border-bottom: 1px solid #2e3438;"
    ));

    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(16, 8, 16, 8);
    headerLayout->setSpacing(16);

    m_lblTitle = new QLabel(headerWidget);
    m_lblTitle->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 15px; font-weight: bold;"));
    m_lblTitle->setText(QStringLiteral("🖥️ %1").arg(m_targetName.isEmpty() ? QStringLiteral("Remote Device") : m_targetName));

    m_lblStats = new QLabel(headerWidget);
    m_lblStats->setStyleSheet(QStringLiteral("color: #9aa2a9; font-size: 13px;"));
    m_lblStats->setText(QStringLiteral("Phiên: #%1 | Đang kết nối...").arg(m_sessionId));

    m_btnDisconnect = new QPushButton(headerWidget);
    m_btnDisconnect->setProperty("role", "dangerActionButton");
    m_btnDisconnect->setText(QStringLiteral("Ngắt kết nối"));
    m_btnDisconnect->setFixedHeight(36);
    m_btnDisconnect->setCursor(Qt::PointingHandCursor);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &RemoteDesktopWindow::onDisconnectClicked);

    headerLayout->addWidget(m_lblTitle);
    headerLayout->addWidget(m_lblStats);
    headerLayout->addStretch();
    headerLayout->addWidget(m_btnDisconnect);

    rootLayout->addWidget(headerWidget);

    // Screen viewport
    m_screenWidget = new RemoteScreenWidget(central);
    connect(m_screenWidget, &RemoteScreenWidget::fpsUpdated, this, &RemoteDesktopWindow::onFpsUpdated);
    connect(m_screenWidget, &RemoteScreenWidget::resolutionChanged, this, &RemoteDesktopWindow::onResolutionChanged);

    rootLayout->addWidget(m_screenWidget, 1);
}

void RemoteDesktopWindow::updateFrame(const QImage &image)
{
    if (m_screenWidget) {
        m_screenWidget->updateFrame(image);
    }
}

void RemoteDesktopWindow::setSessionInfo(quint64 sessionId, const QString &targetName)
{
    m_sessionId = sessionId;
    m_targetName = targetName;
    if (m_lblTitle) {
        m_lblTitle->setText(QStringLiteral("🖥️ %1").arg(m_targetName));
    }
    updateStatus(QStringLiteral("Đang hoạt động"));
}

void RemoteDesktopWindow::updateStatus(const QString &statusText)
{
    if (!m_lblStats)
        return;

    QString text = QStringLiteral("Phiên: #%1").arg(m_sessionId);
    if (!m_lastResolution.isEmpty()) {
        text += QStringLiteral(" | %1x%2").arg(m_lastResolution.width()).arg(m_lastResolution.height());
    }
    if (m_lastFps > 0) {
        text += QStringLiteral(" | %1 FPS").arg(m_lastFps);
    }
    text += QStringLiteral(" | %1").arg(statusText);

    m_lblStats->setText(text);
}

void RemoteDesktopWindow::onFpsUpdated(int fps)
{
    m_lastFps = fps;
    updateStatus(QStringLiteral("Đang truyền trực tiếp"));
}

void RemoteDesktopWindow::onResolutionChanged(const QSize &res)
{
    m_lastResolution = res;
    updateStatus(QStringLiteral("Đang truyền trực tiếp"));
}

void RemoteDesktopWindow::onDisconnectClicked()
{
    emit disconnectRequested();
    close();
}

void RemoteDesktopWindow::closeEvent(QCloseEvent *event)
{
    emit disconnectRequested();
    event->accept();
}
