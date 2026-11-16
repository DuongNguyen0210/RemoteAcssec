#include "RemoteDesktopWindow.h"
#include "../Components/RemoteScreenWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QIcon>
#include <QTimer>

RemoteDesktopWindow::RemoteDesktopWindow(quint64 sessionId, const QString &targetName, QWidget *parent)
    : QMainWindow(parent)
    , m_sessionId(sessionId)
    , m_targetName(targetName)
{
    setupUi();
    updateFrameStatus();

    m_frameTimer = new QTimer(this);
    m_frameTimer->setObjectName("frameWatchdog");
    m_frameTimer->setSingleShot(true);
    m_frameTimer->setInterval(5000);
    connect(m_frameTimer, &QTimer::timeout, this, [this] {
        m_frameState = FrameState::Delayed;
        m_lastFps = 0;
        updateFrameStatus();
    });
    m_frameTimer->start();
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
    m_lblStats->setObjectName("remoteFrameStatus");
    m_lblStats->setStyleSheet(QStringLiteral("color: #9aa2a9; font-size: 13px;"));

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
    if (image.isNull() || m_disconnectEmitted)
        return;

    const bool wasLive = m_frameState == FrameState::Live;
    m_frameState = FrameState::Live;
    m_frameTimer->start();
    m_screenWidget->updateFrame(image);
    if (!wasLive)
        updateFrameStatus();
}

void RemoteDesktopWindow::setSessionInfo(quint64 sessionId, const QString &targetName)
{
    m_sessionId = sessionId;
    m_targetName = targetName;
    if (m_lblTitle) {
        m_lblTitle->setText(QStringLiteral("🖥️ %1").arg(m_targetName));
    }
    updateFrameStatus();
}

void RemoteDesktopWindow::updateFrameStatus()
{
    switch (m_frameState) {
    case FrameState::Waiting:
        updateStatus(QStringLiteral("Đang chờ hình ảnh"));
        break;
    case FrameState::Live:
        updateStatus(QStringLiteral("Đang truyền trực tiếp"));
        break;
    case FrameState::Delayed:
        updateStatus(m_lastResolution.isEmpty()
            ? QStringLiteral("Chưa nhận được hình ảnh")
            : QStringLiteral("Hình ảnh chưa cập nhật"));
        break;
    }
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
    updateFrameStatus();
}

void RemoteDesktopWindow::onResolutionChanged(const QSize &res)
{
    m_lastResolution = res;
    updateFrameStatus();
}

void RemoteDesktopWindow::onDisconnectClicked()
{
    close();
}

void RemoteDesktopWindow::closeEvent(QCloseEvent *event)
{
    m_frameTimer->stop();
    m_inCloseEvent = true;
    if (!m_disconnectEmitted) {
        m_disconnectEmitted = true;
        emit disconnectRequested();
    }
    event->accept();
    m_inCloseEvent = false;
}

void RemoteDesktopWindow::closeForSessionEnd()
{
    m_disconnectEmitted = true;
    if (m_screenWidget) m_screenWidget->clearFrame();
    if (!m_inCloseEvent) close();
}
