#ifndef ADMINSESSIONCONTROLLER_H
#define ADMINSESSIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QTimer>

#include "Network/Protocol/RdtpStreamParser.h"

class RelayClient;

class AdminSessionController : public QObject
{
    Q_OBJECT

public:
    explicit AdminSessionController(QObject *parent = nullptr);

    void requestSession(const QString &targetAgentSessionId);
    void requestSession(const QString &targetAgentSessionId, const QString &host, quint16 port);
    void endSession();
    bool isBusy() const { return m_requestPending || m_activeSessionId != 0; }
    bool sendInput(Protocol::MessageType type, const QByteArray &payload);

signals:
    void screenReceived(const Protocol::ProtocolHeader &header, const QByteArray &payload);
    void sessionEnded();
    void sessionEstablished(quint64 sessionId);
    void sessionFailed(const QString &reason);

private slots:
    void onRelayConnected();
    void onRelayDisconnected();
    void onRelayBytesReceived(const QByteArray &data);
    void onRelayError(const QString &message);

private:
    void sendConnectRequest();
    void failPendingRequest(const QString &reason);

    QTimer m_timeout;
    RelayClient *m_relayClient;
    Protocol::RdtpStreamParser m_streamParser;
    QString m_pendingAgentSessionId;
    quint64 m_activeSessionId;
    bool m_connected;
    bool m_connecting;
    bool m_requestPending;
};

#endif
