#ifndef ADMINSESSIONCONTROLLER_H
#define ADMINSESSIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QByteArray>
#include <QHash>
#include <QVector>

#include "Network/Protocol/RdtpStreamParser.h"

class RelayClient;

class AdminSessionController : public QObject
{
    Q_OBJECT

public:
    explicit AdminSessionController(QObject *parent = nullptr);

    void requestSession(const QString &targetAgentSessionId);

signals:
    void sessionEstablished(quint64 remoteSessionId, const QString &agentSessionId );
    void sessionFailed(const QString &reason);
    void requestFailed(const QString &agentSessionId, const QString &reason);

private slots:
    void onRelayConnected();
    void onRelayDisconnected();
    void onRelayBytesReceived(const QByteArray &data);
    void onRelayError(const QString &message);

private:
    static constexpr int MAX_IN_FLIGHT_FRAMES = 4;

    struct FrameAssembly
    {
        quint32 chunkCount = 0;
        quint32 totalFrameSize = 0;
        quint32 receivedChunkCount = 0;
        QVector<QByteArray> chunks;
    };
    void sendConnectRequest();
    void failPendingRequest(const QString &reason);

    RelayClient *m_relayClient;
    Protocol::RdtpStreamParser m_streamParser;
    QHash<quint32, FrameAssembly> m_frameAssemblies;
    QString m_activeAgentSessionId;
    QString m_pendingAgentSessionId;
    quint64 m_activeSessionId;
    bool m_connected;
    bool m_connecting;
    bool m_requestPending;
};

#endif
