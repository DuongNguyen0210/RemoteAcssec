#include "RemoteControlSubsystem.h"

#include "Core/Controllers/AdminSessionController.h"
#include "GUI/RemoteControl/RemoteSessionWindow.h"
#include "ScreenStreamReceiver.h"

#include <QDebug>

RemoteControlSubsystem::RemoteControlSubsystem(QObject *parent)
    : QObject(parent)
    , m_screenReceiver(new ScreenStreamReceiver(this))
    , m_sessionWindow(nullptr)
{
    connect(m_screenReceiver, &ScreenStreamReceiver::frameReady,
            this, [this](const QImage &frame) {
        if (m_sessionWindow)
            m_sessionWindow->setRemoteFrame(frame);
    });
}

RemoteControlSubsystem::~RemoteControlSubsystem()
{
    QObject::disconnect(m_sessionMessageConnection);
    delete m_sessionWindow;
}

void RemoteControlSubsystem::activate(const RemoteControlContext &context)
{
    if (context.sessionId == 0 || !context.sessionController) {
        qWarning() << "[RemoteControlSubsystem] Khong the kich hoat ngu canh phien khong hop le.";
        return;
    }

    QObject::disconnect(m_sessionMessageConnection);
    m_screenReceiver->setActiveSession(context.sessionId);
    m_sessionMessageConnection = connect(
            context.sessionController,
            &AdminSessionController::sessionProtocolReceived,
            m_screenReceiver,
            &ScreenStreamReceiver::handleMessage);

    if (!m_sessionWindow)
        m_sessionWindow = new RemoteSessionWindow();

    m_sessionWindow->show();
    m_sessionWindow->raise();
    m_sessionWindow->activateWindow();
}
