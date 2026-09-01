#include "RemoteSessionWindow.h"

#include "RemoteDesktopView.h"

RemoteSessionWindow::RemoteSessionWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_desktopView(new RemoteDesktopView(this))
{
    setWindowTitle(QStringLiteral("Phiên điều khiển từ xa"));
    setCentralWidget(m_desktopView);
    resize(960, 540);
}

void RemoteSessionWindow::setRemoteFrame(const QImage &frame)
{
    m_desktopView->setRemoteFrame(frame);
}
