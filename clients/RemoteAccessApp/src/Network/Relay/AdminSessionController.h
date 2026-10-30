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
#include <QAbstractSocket>

class RelayClient;

class AdminSessionController : public QObject
{
    Q_OBJECT

public:
    explicit AdminSessionController(QObject *parent = nullptr);

    void requestSession(const QString &targetAgentSessionId);

signals:
    void frameReady(quint64 sessionId, const QImage &image);
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
    static constexpr int MAX_IN_FLIGHT_FRAMES = 4;
    static constexpr quint32 MAX_FRAME_BYTES = 8 * 1024 * 1024;
    static constexpr quint64 MAX_IN_FLIGHT_BYTES = 16 * 1024 * 1024;
    static constexpr quint32 MAX_CHUNKS = 4096;
    static constexpr qint64 FRAME_TTL_MS = 5000;

    struct FrameAssembly
    {
        quint32 chunkCount = 0;
        quint32 totalFrameSize = 0;
        quint32 receivedChunkCount = 0;
        quint32 receivedBytes = 0;
        qint64 createdAt = 0;
        QVector<QByteArray> chunks;
    };
    void handleScreenFrame(const QByteArray &payload);
    void pruneFrames();
    void resetFrames();
    void decodeFrame(quint32 frameId, QByteArray jpeg);
    static bool isNewerFrame(quint32 candidate, quint32 reference);
    void sendConnectRequest();
    void failPendingRequest(const QString &reason);
    void handleFatalRelayTermination(const QString &reason);
    static bool isFatalTransportError(QAbstractSocket::SocketError socketError);

    RelayClient *m_relayClient;
    Protocol::RdtpStreamParser m_streamParser;
    QHash<quint32, FrameAssembly> m_frameAssemblies;
    QElapsedTimer m_frameClock;
    quint64 m_reservedFrameBytes = 0;
    quint64 m_generation = 0;
    bool m_decoding = false;
    bool m_hasCompletedFrame = false;
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
