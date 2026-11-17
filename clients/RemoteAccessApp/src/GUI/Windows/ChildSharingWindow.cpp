#include "ChildSharingWindow.h"

#include <QCloseEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ChildSharingWindow::ChildSharingWindow(QWidget *parent)
    : QWidget(parent)
    , m_status(new QLabel(this))
    , m_toggle(new QPushButton(this))
{
    setWindowTitle(QStringLiteral("RemoteAcssec — Máy CHILD"));
    setMinimumWidth(360);
    m_status->setObjectName("childSharingStatus");
    m_status->setWordWrap(true);
    m_toggle->setObjectName("childSharingToggle");
    m_toggle->setProperty("role", "primaryActionButton");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    layout->addWidget(m_status);
    layout->addWidget(m_toggle, 0, Qt::AlignLeft);

    connect(m_toggle, &QPushButton::clicked, this, [this] {
        if (m_state == State::Paused) {
            m_state = State::Ready;
            updateControls();
            emit sharingEnabledChanged(true);
        } else {
            stopSharing();
        }
    });
    updateControls();
}

void ChildSharingWindow::sessionStarted()
{
    if (m_state != State::Ready)
        return;
    m_state = State::Sharing;
    updateControls();
    if (isMinimized())
        showNormal();
    raise();
}

void ChildSharingWindow::sessionEnded()
{
    if (m_state != State::Sharing)
        return;
    m_state = State::Ready;
    updateControls();
}

void ChildSharingWindow::authenticationLost()
{
    m_state = State::SignedOut;
    updateControls();
}

void ChildSharingWindow::stopSharing()
{
    if (m_state != State::Ready && m_state != State::Sharing)
        return;
    m_state = State::Paused;
    updateControls();
    emit sharingEnabledChanged(false);
}

void ChildSharingWindow::closeEvent(QCloseEvent *event)
{
    stopSharing();
    event->accept();
}

void ChildSharingWindow::updateControls()
{
    switch (m_state) {
    case State::Ready:
        m_status->setText(QStringLiteral("Chưa chia sẻ màn hình."));
        break;
    case State::Sharing:
        m_status->setText(QStringLiteral("Màn hình đang được chia sẻ."));
        break;
    case State::Paused:
        m_status->setText(QStringLiteral("Đã dừng chia sẻ màn hình."));
        break;
    case State::SignedOut:
        m_status->setText(QStringLiteral("Phiên đăng nhập đã hết hạn. Hãy mở lại ứng dụng để đăng nhập."));
        break;
    }
    m_toggle->setText(m_state == State::Paused ? QStringLiteral("Bật lại chia sẻ")
                                              : QStringLiteral("Dừng chia sẻ"));
    m_toggle->setEnabled(m_state != State::SignedOut);
}
