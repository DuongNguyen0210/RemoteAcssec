#include "ChildStatusWindow.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ChildStatusWindow::ChildStatusWindow(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("childStatusWindow"));
    setWindowTitle(QStringLiteral("RemoteAccess — máy được điều khiển"));
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
}

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
    m_toggle->setText(enabled ? QStringLiteral("Dừng chia sẻ") : QStringLiteral("Bật chia sẻ"));
}
