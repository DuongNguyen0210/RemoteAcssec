#include "RemoteControlSubsystem.h"

#include "Core/Controllers/AdminSessionController.h"
#include "Core/Services/RemoteControl/Input/RemoteInputReceiver.h"
#include "Core/Services/RemoteControl/Input/RemoteInputSender.h"
#include "Core/Services/Screen/ScreenStreamSender.h"
#include "GUI/RemoteControl/RemoteDesktopView.h"
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
    QObject::disconnect(m_mouseInputConnection);
    QObject::disconnect(m_childSessionMessageConnection);
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

    if (!m_inputSender)
        m_inputSender = new RemoteInputSender(this);
    m_inputSender->setSessionController(context.sessionController);

    if (!m_sessionWindow)
        m_sessionWindow = new RemoteSessionWindow();

    QObject::disconnect(m_mouseInputConnection);
    m_mouseInputConnection = connect(
            m_sessionWindow->desktopView(),
            &RemoteDesktopView::remoteMouseInput,
            this,
            &RemoteControlSubsystem::submitMouseInput);

    m_sessionWindow->show();
    m_sessionWindow->raise();
    m_sessionWindow->activateWindow();
}

void RemoteControlSubsystem::activate(const ChildRemoteControlContext &context)
{
    if (!context.screenStreamSender) {
        qWarning() << "[RemoteControlSubsystem] Khong the kich hoat CHILD input khong hop le.";
        return;
    }

    if (!m_inputReceiver)
        m_inputReceiver = new RemoteInputReceiver(this);

    m_inputReceiver->setActiveSession(
            context.screenStreamSender->currentSessionId());
    QObject::disconnect(m_childSessionMessageConnection);
    m_childSessionMessageConnection = connect(
            context.screenStreamSender,
            &ScreenStreamSender::sessionProtocolReceived,
            this,
            [this, sender = context.screenStreamSender](
                    const Protocol::RdtpStreamParser::Message &message) {
        m_inputReceiver->setActiveSession(sender->currentSessionId());
        m_inputReceiver->handleMessage(message);
    });
}

void RemoteControlSubsystem::submitMouseInput(const RemoteMouseEvent &event)
{
    if (m_inputSender)
        m_inputSender->submitMouseInput(event);
}
