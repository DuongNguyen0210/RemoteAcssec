#pragma once

#include <QObject>
#include <QTimer>

#include <optional>

#include "RemoteMouseEvent.h"

class AdminSessionController;

class RemoteInputSender : public QObject
{
    Q_OBJECT

public:
    static constexpr int MOVE_COALESCE_INTERVAL_MS = 16;

    explicit RemoteInputSender(QObject *parent = nullptr);

    void setSessionController(AdminSessionController *sessionController);
    void submitMouseInput(const RemoteMouseEvent &event);

private slots:
    void flushPendingMove();

private:
    void sendImmediately(const RemoteMouseEvent &event);

    AdminSessionController *m_sessionController;
    QTimer m_moveTimer;
    std::optional<RemoteMouseEvent> m_pendingMove;
};
