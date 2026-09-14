#ifndef SCREENSTREAMSENDER_H
#define SCREENSTREAMSENDER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <cstdint>

#include "Network/Protocol/RdtpStreamParser.h"

class RelayClient;

class ScreenStreamSender : public QObject
{
    Q_OBJECT

public:
    static constexpr int STREAM_INTERVAL_MS = 200;
    static constexpr qint64 MAX_PENDING_BYTES = 2 * 524288LL;
    static constexpr quint16 RELAY_PORT = 8080;

    explicit ScreenStreamSender(const QString &childUsername, QObject *parent = nullptr);
    ~ScreenStreamSender();

    void start();
    void stop();

private slots:
    void onTick();
    void onRelayConnected();
    void onRelayDisconnected();
    void onRelayBytesReceived(const QByteArray &data);

private:
    void sendRegisterHost();
    void handleSessionRequest(const Protocol::RdtpStreamParser::Message &message);
    bool sendSessionResponse(Protocol::MessageType type, uint64_t sessionId);

    QTimer      *m_timer;
    RelayClient *m_relayClient;
    uint32_t     m_frameId;
    QString      m_childUsername;
    bool         m_running = false;
    bool         m_registered;
    uint64_t     m_currentSessionId;
    Protocol::RdtpStreamParser m_streamParser;
};

#endif
