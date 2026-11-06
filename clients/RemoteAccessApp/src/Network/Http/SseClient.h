#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTimer>

class QNetworkReply;

/**
 * @brief Generic Server-Sent Events (SSE) Client for Qt.
 *
 * Implements the W3C Server-Sent Events stream parsing over HTTP.
 * Keeps an open GET connection to the specified endpoint, parses raw chunks
 * into distinct events (bounded by double-newline "\n\n"), and emits
 * @c eventReceived(eventName, payload) for consumers.
 *
 * It is completely domain-agnostic (does not know about devices, accounts, etc.)
 * and can be reused for any SSE endpoint in the application.
 *
 * Handles automatic reconnect with backoff/interval when the stream is closed
 * by the server or interrupted by network errors.
 */
class SseClient : public QObject
{
    Q_OBJECT

public:
    static constexpr int DEFAULT_RECONNECT_DELAY_MS = 3000;

    explicit SseClient(const QString &endpoint, QObject *parent = nullptr);
    ~SseClient() override;

    /** Starts the SSE stream. Safe to call multiple times (no-op if already running). */
    void start();

    /** Stops the stream and cancels pending reconnects. */
    void stop();

    bool isRunning() const { return m_running; }
    QString endpoint() const { return m_endpoint; }
    void setEndpoint(const QString &endpoint);

signals:
    /**
     * Emitted when a complete event block is received.
     * @param event The event type (from "event: <name>"), or empty string if omitted.
     * @param data  The raw payload bytes accumulated from one or more "data: <bytes>" lines.
     */
    void eventReceived(const QString &event, const QByteArray &data);

    /** Emitted when HTTP stream connection is active and receiving data. */
    void connected();

    /** Emitted when connection drops (reconnect will trigger unless stop() was called). */
    void disconnected();

private slots:
    void openConnection();
    void onReadyRead();
    void onFinished();

private:
    void parseBuffer();
    void scheduleReconnect();
    void abortReply();

    QString        m_endpoint;
    QNetworkReply *m_reply          = nullptr;
    QTimer        *m_reconnectTimer = nullptr;
    QByteArray     m_buffer;
    bool           m_running        = false;
    bool           m_hasEmittedConnected = false;
};
