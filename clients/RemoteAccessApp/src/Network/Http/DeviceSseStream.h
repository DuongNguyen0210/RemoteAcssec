#pragma once

#include <QObject>
#include <QTimer>
#include <QByteArray>
#include <QList>
#include "Domain/Model/DeviceInfo.h"

class QNetworkReply;

/**
 * @brief Consumes the server's Server-Sent Events stream for device presence.
 *
 * Opens a persistent HTTP GET connection to @c /api/v1/devices/events and
 * reads the @c text/event-stream protocol.  Whenever a complete SSE event
 * named @c device-list arrives, it is parsed and emitted as
 * @c devicesReceived().
 *
 * The connection automatically re-establishes itself after @c RECONNECT_DELAY_MS
 * if the server closes it (e.g. after the 60-second server-side timeout).
 *
 * Usage:
 * @code
 *   auto *stream = new DeviceSseStream(this);
 *   connect(stream, &DeviceSseStream::devicesReceived,
 *           store,  &DeviceStore::replaceDevices);
 *   stream->start();
 *   // ... later:
 *   stream->stop();
 * @endcode
 */
class DeviceSseStream : public QObject
{
    Q_OBJECT

public:
    static constexpr int RECONNECT_DELAY_MS = 3000;

    explicit DeviceSseStream(QObject *parent = nullptr);
    ~DeviceSseStream() override;

    /** Opens the SSE connection. Safe to call multiple times (no-op if already running). */
    void start();

    /** Closes the stream and disables automatic reconnect. */
    void stop();

    bool isRunning() const { return m_running; }

signals:
    /**
     * Emitted whenever a full @c device-list SSE event is received and parsed.
     * This signal is compatible with @c DeviceStore::replaceDevices and
     * @c DeviceService::devicesResult (success=true variant).
     */
    void devicesReceived(const QList<DeviceInfo> &devices);

    /** Emitted when the SSE connection is established (initial snapshot sent by server). */
    void connected();

    /** Emitted when the SSE connection is lost (will reconnect unless stop() was called). */
    void disconnected();

private slots:
    void openConnection();
    void onReadyRead();
    void onFinished();

private:
    void parseBuffer();
    void scheduleReconnect();
    void abortReply();

    QNetworkReply *m_reply    = nullptr;
    QTimer        *m_reconnectTimer;
    QByteArray     m_buffer;          ///< Accumulates partial SSE chunks
    bool           m_running  = false;
};
