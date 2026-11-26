#include "ChildStatusWindow.h"
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

ChildStatusWindow::ChildStatusWindow(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("childStatusWindow"));
    setWindowTitle(QStringLiteral("RemoteAccess — máy được điều khiển"));
#ifdef Q_OS_WIN
    // Native caption buttons can wait for mouse release on the same thread
    // that must read that release from the relay.
    setWindowFlags(windowFlags().setFlag(Qt::CustomizeWindowHint)
                     .setFlag(Qt::WindowTitleHint).setFlag(Qt::WindowSystemMenuHint)
                     .setFlag(Qt::WindowCloseButtonHint)
                     .setFlag(Qt::WindowMinimizeButtonHint, false)
                     .setFlag(Qt::WindowMaximizeButtonHint, false));
    // Qt's Windows backend adds the native maximize button for an unrestricted
    // window. A finite maximum keeps it disabled and still allows normal resizing.
    setMaximumSize(QWIDGETSIZE_MAX - 1, QWIDGETSIZE_MAX - 1);
#endif
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_StyledBackground);
    setMinimumWidth(420);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto *title = new QLabel(QStringLiteral("Chia sẻ máy tính"), this);
    title->setProperty("role", "sectionTitle");
    m_status = new QLabel(QStringLiteral("Chia sẻ đang tắt. Bật chia sẻ để cho phép kết nối không giám sát."), this);
    m_status->setObjectName(QStringLiteral("childSessionStatus"));
    m_status->setProperty("role", "pageSubtitle");
    m_status->setWordWrap(true);
    m_toggle = new QPushButton(QStringLiteral("Bật chia sẻ"), this);
    m_toggle->setObjectName(QStringLiteral("stopSharingButton"));
    m_toggle->setProperty("role", "primaryActionButton");
    m_toggle->setMinimumHeight(40);
    layout->addWidget(title);
    layout->addWidget(m_status);
    layout->addWidget(m_toggle);
    connect(m_toggle, &QPushButton::clicked, this, &ChildStatusWindow::sharingToggleRequested);
#ifdef Q_OS_WIN
    auto *windowActions = new QHBoxLayout;
    auto *minimize = new QPushButton(QStringLiteral("Thu nhỏ"), this);
    minimize->setObjectName(QStringLiteral("childMinimizeButton"));
    m_windowSizeButton = new QPushButton(QStringLiteral("Phóng to"), this);
    m_windowSizeButton->setObjectName(QStringLiteral("childWindowSizeButton"));
    windowActions->addStretch();
    windowActions->addWidget(minimize);
    windowActions->addWidget(m_windowSizeButton);
    layout->addLayout(windowActions);
    connect(minimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_windowSizeButton, &QPushButton::clicked, this, [this] {
        if (isMaximized()) showNormal();
        else showMaximized();
    });
#endif
}

#ifdef Q_OS_WIN
void ChildStatusWindow::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && m_windowSizeButton)
        m_windowSizeButton->setText(isMaximized() ? QStringLiteral("Khôi phục") : QStringLiteral("Phóng to"));
}
#endif

void ChildStatusWindow::showRelayReady() {
    m_status->setText(QStringLiteral("Sẵn sàng nhận phiên điều khiển không giám sát."));
}
void ChildStatusWindow::showSessionStarted() {
    m_status->setText(QStringLiteral("Đang có phiên điều khiển từ xa. Bạn có thể dừng chia sẻ bất cứ lúc nào."));
}
void ChildStatusWindow::showSessionEnded(bool relayReady) {
    m_status->setText(relayReady
            ? QStringLiteral("Phiên đã kết thúc. Máy đang sẵn sàng nhận phiên mới.")
            : QStringLiteral("Phiên đã kết thúc. Đang kết nối lại Relay."));
}
void ChildStatusWindow::showRelayFailure() {
    m_status->setText(QStringLiteral("Mất kết nối Relay. Đang thử kết nối lại."));
}
void ChildStatusWindow::showAuthenticationLost() {
    m_status->setText(QStringLiteral("Phiên đăng nhập đã hết hạn. Đóng cửa sổ để thoát."));
    m_toggle->setEnabled(false);
}
void ChildStatusWindow::showSharingEnabled(bool enabled) {
    m_status->setText(enabled
            ? QStringLiteral("Đang kết nối Relay để sẵn sàng nhận phiên điều khiển.")
            : QStringLiteral("Đã dừng chia sẻ. Bạn có thể bật lại hoặc đóng cửa sổ."));
    m_toggle->setProperty("role", enabled ? "dangerActionButton" : "primaryActionButton");
    m_toggle->style()->unpolish(m_toggle);
    m_toggle->style()->polish(m_toggle);
    m_toggle->setText(enabled ? QStringLiteral("Dừng chia sẻ") : QStringLiteral("Bật chia sẻ"));
}
