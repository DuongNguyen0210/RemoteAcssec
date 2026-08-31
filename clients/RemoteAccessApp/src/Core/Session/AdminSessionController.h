#ifndef ADMINSESSIONCONTROLLER_H
#define ADMINSESSIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QtGlobal>

#include "../../Network/protocol/RdtpStreamParser.h"

class RelayClient;

class AdminSessionController : public QObject
{
    Q_OBJECT

public:
    explicit AdminSessionController(QObject *parent = nullptr);

    void requestSession(const QString &targetChildUsername);

signals:
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

    RelayClient *m_relayClient;
    Protocol::RdtpStreamParser m_streamParser;
    QString m_pendingTargetUsername;
    quint64 m_activeSessionId;
    bool m_connected;
    bool m_connecting;
    bool m_requestPending;
};

#endif // ADMINSESSIONCONTROLLER_H
