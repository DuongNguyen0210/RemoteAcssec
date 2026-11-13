#ifndef ADMINSESSIONCONTROLLER_H
#define ADMINSESSIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QByteArray>
#include <QHash>
#include <QVector>
#include <QImage>
#include <QElapsedTimer>

#include "Network/Protocol/RdtpStreamParser.h"
#include "Network/Protocol/ScreenFrameReassembler.h"
#include <QAbstractSocket>

class RelayClient;

class AdminSessionController : public QObject
{
    Q_OBJECT

public:
    explicit AdminSessionController(QObject *parent = nullptr);

    void requestSession(const QString &targetAgentSessionId);
    void disconnectSession();

signals:
    void frameReady(quint64 sessionId, const QImage &image);
    void frameReceived(quint32 frameId, const QByteArray &jpegData);
    void sessionEstablished(quint64 remoteSessionId, const QString &agentSessionId );
    void sessionFailed(const QString &reason);
    void requestFailed(const QString &agentSessionId, const QString &reason);
    void sessionEnded(quint64 remoteSessionId, const QString &agentSessionId, const QString &reason);

private slots:
    void onRelayConnected();
    void onRelayDisconnected();
    void onRelayBytesReceived(const QByteArray &data);
    void onRelayError(const QString &message, QAbstractSocket::SocketError socketError);

private:
    friend class AdminScreenViewerTest;
    void pruneFrames();
    void resetFrames();
    void decodeFrame(quint32 frameId, QByteArray jpeg);
    void sendConnectRequest();
    void failPendingRequest(const QString &reason);
    void handleFatalRelayTermination(const QString &reason);
    static bool isFatalTransportError(QAbstractSocket::SocketError socketError);

    RelayClient *m_relayClient;
    Protocol::RdtpStreamParser m_streamParser;
    Protocol::ScreenFrameReassembler m_frameReassembler;
    quint64 m_generation = 0;
    bool m_decoding = false;
    quint32 m_latestCompletedFrame = 0;
    QByteArray m_pendingJpeg;
    quint32 m_pendingFrameId = 0;
    QString m_activeAgentSessionId;
    QString m_pendingAgentSessionId;
    quint64 m_activeSessionId;
    bool m_connected;
    bool m_connecting;
    bool m_requestPending;
};

#endif
