#pragma once

#include <QMetaObject>
#include <QObject>

#include "ChildRemoteControlContext.h"
#include "RemoteControlContext.h"
#include "Input/RemoteMouseEvent.h"

class RemoteInputReceiver;
class RemoteInputSender;
class RemoteSessionWindow;
class ScreenStreamReceiver;

class RemoteControlSubsystem : public QObject
{
    Q_OBJECT

public:
    explicit RemoteControlSubsystem(QObject *parent = nullptr);
    ~RemoteControlSubsystem() override;

    void activate(const RemoteControlContext &context);
    void activate(const ChildRemoteControlContext &context);

private slots:
    void submitMouseInput(const RemoteMouseEvent &event);

private:
    ScreenStreamReceiver *m_screenReceiver;
    RemoteSessionWindow *m_sessionWindow;
    QMetaObject::Connection m_sessionMessageConnection;
    RemoteInputSender *m_inputSender = nullptr;
    RemoteInputReceiver *m_inputReceiver = nullptr;
    QMetaObject::Connection m_mouseInputConnection;
    QMetaObject::Connection m_childSessionMessageConnection;
};
