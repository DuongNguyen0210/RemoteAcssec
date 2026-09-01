#pragma once

#include <QtGlobal>

class AdminSessionController;

struct RemoteControlContext
{
    quint64 sessionId = 0;
    AdminSessionController *sessionController = nullptr;
};
