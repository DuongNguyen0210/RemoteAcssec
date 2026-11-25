#include "Network/Http/ApiClient.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "Network/Relay/AdminSessionController.h"
#include "Network/Relay/ChildSessionController.h"
#include "Network/Relay/StaticRelayEndpointProvider.h"
#include "Streaming/ScreenStreamSender.h"

#include <QPainter>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>
#include <QWidget>
#include <algorithm>

// Move a fixed texture so successive captures have comparable JPEG cost.
class MovingDesktop : public QWidget {
public:
    MovingDesktop() : m_texture(720, 650, QImage::Format_RGB32) {
        QRandomGenerator random(47);
        for (int y = 0; y < m_texture.height(); ++y) {
            auto *row = reinterpret_cast<QRgb *>(m_texture.scanLine(y));
            for (int x = 0; x < m_texture.width(); ++x)
                row[x] = 0xff000000u | random.generate();
        }
        resize(700, 650);
        connect(&m_animation, &QTimer::timeout, this, [this] {
            m_offset = (m_offset + 1) % 20;
            update();
        });
        m_animation.start(55);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.drawImage(-m_offset, 0, m_texture);
    }
private:
    QImage m_texture;
    int m_offset = 0;
    QTimer m_animation;
};

class AdaptiveStreamRelayTest : public QObject {
    Q_OBJECT
    using Type = Protocol::MessageType;
    static QByteArray control(Type type, quint64 session, QByteArray payload = {}, quint16 flags = 0) {
        Protocol::ProtocolHeader header(type);
        header.sessionId = session;
        header.flags = flags;
        header.payloadLength = payload.size();
        return Protocol::ProtocolSerializer::serializeHeader(header) + payload;
    }

private slots:
    void preservesResolutionWhenRelayDelayRisesAndRecovers() {
        constexpr quint64 session = 371;
        qputenv("REMOTE_STREAM_KBPS", "8000");
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        StaticRelayEndpointProvider childEndpoint("127.0.0.1", server.serverPort());
        ChildSessionController child(&childEndpoint);
        ScreenStreamSender sender(&child);
        AdminSessionController admin(new StaticRelayEndpointProvider("127.0.0.1", server.serverPort()), nullptr);
        MovingDesktop desktop;
        desktop.show();
        ApiClient::instance().setToken("loopback-stream-test");
        child.start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *childPeer = server.nextPendingConnection();
        QTRY_VERIFY(childPeer->bytesAvailable() > 0);
        childPeer->readAll();
        childPeer->write(control(Type::REGISTER_ACK, 0, QByteArray(1, 1)));
        QTRY_VERIFY(child.relayReady());
        admin.requestSession("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
        QTRY_VERIFY(server.hasPendingConnections());
        auto *adminPeer = server.nextPendingConnection();
        QTRY_VERIFY(adminPeer->bytesAvailable() > 0);
        adminPeer->readAll();

        QObject connections;
        const auto capability = control(Type::SCREEN_FRAME_ACK, session, {}, Protocol::SCREEN_FRAME_ACK_CAPABILITY_FLAG);
        int addedDelayMs = 0;
        Protocol::RdtpStreamParser parser;
        bool awaitingAck = false;
        qint64 wireBytes = 0;
        connect(childPeer, &QTcpSocket::readyRead, &connections, [&] {
            const auto result = parser.feed(childPeer->readAll());
            QCOMPARE(result.error, Protocol::RdtpStreamParser::Error::None);
            for (const auto &message : result.messages) {
                if (message.header.type == Type::SESSION_ACCEPT) {
                    adminPeer->write(control(Type::CONNECT_RESULT, session, QByteArray(1, 1)) + capability);
                    continue;
                }
                QCOMPARE(message.header.type, Type::SCREEN_FRAME);
                QVERIFY(!awaitingAck);
                awaitingAck = true;
                const auto packet = Protocol::ProtocolSerializer::serializeHeader(message.header) + message.payload;
                QVERIFY(packet.size() <= ScreenStreamSender::MAX_WIRE_FRAME_BYTES);
                wireBytes += packet.size();
                QTimer::singleShot(addedDelayMs, &connections, [adminPeer, packet] { adminPeer->write(packet); });
            }
        });
        Protocol::RdtpStreamParser acks;
        connect(adminPeer, &QTcpSocket::readyRead, &connections, [&] {
            const auto result = acks.feed(adminPeer->readAll());
            QCOMPARE(result.error, Protocol::RdtpStreamParser::Error::None);
            for (const auto &message : result.messages) {
                QCOMPARE(message.header.type, Type::SCREEN_FRAME_ACK);
                QVERIFY(awaitingAck);
                awaitingAck = false;
                childPeer->write(Protocol::ProtocolSerializer::serializeHeader(message.header));
            }
        });
        QVector<int> widths;
        connect(&admin, &AdminSessionController::frameReady, &connections, [&](quint64 id, const QImage &image) {
            QCOMPARE(id, session);
            QVERIFY(!image.isNull());
            widths.append(image.width());
        });
        childPeer->write(capability + control(Type::SESSION_REQUEST, session));
        QTRY_VERIFY_WITH_TIMEOUT(!widths.isEmpty(), 4000);
        widths.clear();
        const auto startBytes = wireBytes;
        QElapsedTimer interval;
        interval.start();
        QTest::qWait(4000);
        const qint64 elapsed = interval.elapsed();
        QVERIFY(widths.size() >= 10);
        QVERIFY(wireBytes - startBytes <= 1000000 * elapsed / 1000 + ScreenStreamSender::MAX_WIRE_FRAME_BYTES);
        const int healthy = widths.last();
        qInfo() << "healthy frames=" << widths.size() << "width=" << healthy
                << "RDTP bytes/s=" << (wireBytes - startBytes) * 1000 / elapsed;

        widths.clear();
        addedDelayMs = 1200;
        const auto delayedStart = interval.elapsed();
        QTRY_VERIFY_WITH_TIMEOUT(widths.size() >= 3, 6500);
        const auto delayedElapsed = interval.elapsed() - delayedStart;
        QVERIFY(delayedElapsed >= 2400);
        QVERIFY(*std::min_element(widths.cbegin(), widths.cend()) >= healthy * 95 / 100);
        qInfo() << "delayed frames=" << widths.size() << "width=" << widths.last()
                << "elapsed ms=" << delayedElapsed;

        widths.clear();
        addedDelayMs = 0;
        QTest::qWait(3200);
        QVERIFY(widths.size() >= 6);
        QVERIFY(*std::min_element(widths.cbegin(), widths.cend()) >= healthy * 95 / 100);
        child.stop();
    }
};

QTEST_MAIN(AdaptiveStreamRelayTest)
#include "AdaptiveStreamRelayTest.moc"
