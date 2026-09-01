#pragma once

#include <QMetaObject>
#include <QObject>

#include "RemoteControlContext.h"

class RemoteSessionWindow;
class ScreenStreamReceiver;

class RemoteControlSubsystem : public QObject
{
    Q_OBJECT

public:
    explicit RemoteControlSubsystem(QObject *parent = nullptr);
    ~RemoteControlSubsystem() override;

    void activate(const RemoteControlContext &context);

private:
    ScreenStreamReceiver *m_screenReceiver;
    RemoteSessionWindow *m_sessionWindow;
    QMetaObject::Connection m_sessionMessageConnection;
};
