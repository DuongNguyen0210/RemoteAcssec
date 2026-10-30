#include <QtTest>
#include <QBuffer>
#include <QtEndian>
#include "Network/Relay/AdminSessionController.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "GUI/Pages/SessionsPage.h"
#include "GUI/Pages/DevicesPage.h"
#include "GUI/Pages/AccountPage.h"
#include "GUI/Windows/MainWindow.h"
#include <QLineEdit>
#include <QStackedWidget>
#include <QPushButton>

class AdminScreenViewerTest : public QObject {
    Q_OBJECT
    QByteArray jpeg(QColor color = Qt::red) {
        QImage image(32, 24, QImage::Format_RGB32);
        image.fill(color);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "JPEG")) return {};
        return bytes;
    }
    QByteArray packet(quint64 session, quint32 id, quint32 index, quint32 count, quint32 size, const QByteArray &part) {
        QByteArray payload(16, '\0');
        auto *p = reinterpret_cast<uchar *>(payload.data());
        qToBigEndian(id, p); qToBigEndian(index, p + 4); qToBigEndian(count, p + 8); qToBigEndian(size, p + 12);
        payload += part;
        Protocol::ProtocolHeader h(Protocol::MessageType::SCREEN_FRAME);
        h.sessionId = session; h.payloadLength = quint32(payload.size());
        return Protocol::ProtocolSerializer::serializeHeader(h) + payload;
    }
    void feed(AdminSessionController &c, quint64 session, quint32 id, quint32 index, quint32 count, quint32 size, const QByteArray &part) {
        c.onRelayBytesReceived(packet(session, id, index, count, size, part));
    }
private slots:
    void sessionNavigationKeepsSidebarAndTopbarInSync() {
        MainWindow window(new DevicesPage, new AccountPage);
        window.show();
        auto *stack = window.findChild<QStackedWidget *>("contentStack");
        auto *search = window.findChild<QLineEdit *>("topbarSearchInput");
        auto *sidebar = window.findChild<SidebarWidget *>("sidebar");
        QVERIFY(stack && search && sidebar);
        QPushButton *sessions = nullptr;
        QPushButton *devices = nullptr;
        for (auto *button : sidebar->findChildren<QPushButton *>()) {
            if (button->property("pageIndex") == 1) sessions = button;
            if (button->property("pageIndex") == 0) devices = button;
        }
        QVERIFY(sessions && devices);
        QSignalSpy changed(&window, &MainWindow::pageSelected);
        window.showRemoteSession();
        QCOMPARE(stack->currentIndex(), 1);
        QVERIFY(qobject_cast<SessionsPage *>(stack->currentWidget()));
        QVERIFY(sessions->isChecked());
        QCOMPARE(search->placeholderText(), QStringLiteral("Tìm phiên kết nối — Sắp có"));
        QCOMPARE(changed.count(), 1);
        QTest::mouseClick(devices, Qt::LeftButton);
        QCOMPARE(stack->currentIndex(), 0);
        QVERIFY(devices->isChecked());
        QCOMPARE(search->placeholderText(), QStringLiteral("Tìm thiết bị — Sắp có"));
        window.showRemoteSession();
        QCOMPARE(stack->currentIndex(), 1);
        QVERIFY(sessions->isChecked());
        QCOMPARE(search->placeholderText(), QStringLiteral("Tìm phiên kết nối — Sắp có"));
    }
    void singleChunkAndWidget() {
        AdminSessionController c; c.m_activeSessionId = 42;
        QSignalSpy spy(&c, &AdminSessionController::frameReady);
        auto bytes = jpeg(); QVERIFY(!bytes.isEmpty());
        feed(c, 42, 1, 0, 1, bytes.size(), bytes);
        QTRY_COMPARE(spy.count(), 1);
        QImage image = qvariant_cast<QImage>(spy.at(0).at(1));
        QCOMPARE(image.size(), QSize(32, 24));
        SessionsPage page; page.resize(320,240); page.setFrame(image); page.show();
        QTest::qWait(20);
        int metricCards = 0;
        for (auto *widget : page.findChildren<QWidget *>())
            if (widget->property("role") == "metricCard") ++metricCards;
        QCOMPARE(metricCards, 3);
        auto *viewer = page.findChild<QWidget *>("screenViewer"); QVERIFY(viewer);
        auto shown = viewer->grab().toImage();
        QVERIFY(shown.pixelColor(shown.width()/2,shown.height()/2).red() > 180);
        page.clearFrame();
        QVERIFY(viewer->grab().toImage() != shown);
    }
    void orderedAndOutOfOrderDuplicate() {
        AdminSessionController c; c.m_activeSessionId = 42; QSignalSpy spy(&c, &AdminSessionController::frameReady);
        auto b = jpeg(); auto a=b.left(b.size()/2); auto z=b.mid(b.size()/2);
        feed(c,42,1,0,2,b.size(),a); feed(c,42,1,0,2,b.size(),a);
        QCOMPARE(spy.count(),0); QCOMPARE(c.m_frameAssemblies[1].receivedChunkCount,quint32(1));
        feed(c,42,1,1,2,b.size(),z); QTRY_COMPARE(spy.count(),1);
        feed(c,42,2,1,2,b.size(),z); feed(c,42,2,0,2,b.size(),a); QTRY_COMPARE(spy.count(),2);
    }
    void malformedAndDifferentSessions() {
        AdminSessionController c; c.m_activeSessionId=42; QSignalSpy spy(&c,&AdminSessionController::frameReady);
        auto b=jpeg(); auto a=b.left(b.size()/2); auto z=b.mid(b.size()/2);
        feed(c,43,1,0,1,b.size(),b); feed(c,42,2,0,500000,999999999,a);
        feed(c,42,3,0,2,b.size(),a); feed(c,42,3,1,3,b.size(),z);
        feed(c,42,4,0,2,b.size()+1,a); feed(c,42,4,1,2,b.size()+1,z);
        feed(c,42,5,0,1,3,QByteArray("abc"));
        QTest::qWait(50); QCOMPARE(spy.count(),0); QVERIFY(!c.m_frameAssemblies.contains(3));
        QVERIFY(!c.m_frameAssemblies.contains(4));
    }
    void interleavedAndReset() {
        AdminSessionController c; c.m_activeSessionId=42; QSignalSpy spy(&c,&AdminSessionController::frameReady);
        auto b=jpeg(); auto a=b.left(b.size()/2); auto z=b.mid(b.size()/2);
        feed(c,42,1,0,2,b.size(),a); feed(c,42,2,1,2,b.size(),z);
        feed(c,42,1,1,2,b.size(),z); QTRY_COMPARE(spy.count(),1);
        feed(c,42,2,0,2,b.size(),a); QTRY_COMPARE(spy.count(),2);
        feed(c,42,3,0,2,b.size(),a); c.handleFatalRelayTermination(QStringLiteral("test"));
        c.m_activeSessionId=43; feed(c,43,3,1,2,b.size(),z); QCOMPARE(spy.count(),2);
        feed(c,42,4,0,1,b.size(),b); QCOMPARE(spy.count(),2);
        feed(c,43,3,0,2,b.size(),a); QTRY_COMPARE(spy.count(),3);
    }
    void boundedAndStale() {
        AdminSessionController c; c.m_activeSessionId=42;
        for (int i=0;i<5;++i) feed(c,42,i,0,2,8*1024*1024,QByteArray("x"));
        QCOMPARE(c.m_frameAssemblies.size(),2); QCOMPARE(c.m_reservedFrameBytes,quint64(16*1024*1024));
        for (auto it=c.m_frameAssemblies.begin();it!=c.m_frameAssemblies.end();++it) it->createdAt=-6000;
        c.pruneFrames(); QCOMPARE(c.m_frameAssemblies.size(),0); QCOMPARE(c.m_reservedFrameBytes,quint64(0));
    }
    void pendingDecodeCannotLeakAcrossSession() {
        AdminSessionController c; c.m_activeSessionId=42;
        QSignalSpy images(&c,&AdminSessionController::frameReady);
        auto b=jpeg();
        feed(c,42,1,0,1,b.size(),b);
        QVERIFY(c.m_decoding);
        c.handleFatalRelayTermination(QStringLiteral("test"));
        c.m_activeSessionId=43;
        feed(c,43,2,0,1,b.size(),b);
        QTRY_COMPARE(images.count(),1);
        QCOMPARE(images.at(0).at(0).toULongLong(),quint64(43));
    }
    void newerCompletionRejectsOlderAndReplay() {
        AdminSessionController c; c.m_activeSessionId=42;
        QSignalSpy images(&c,&AdminSessionController::frameReady);
        auto b=jpeg(); auto a=b.left(b.size()/2); auto z=b.mid(b.size()/2);
        feed(c,42,10,0,2,b.size(),a);
        feed(c,42,11,0,1,b.size(),b);
        QTRY_COMPARE(images.count(),1);
        feed(c,42,10,1,2,b.size(),z);
        feed(c,42,11,0,1,b.size(),b);
        QTest::qWait(50);
        QCOMPARE(images.count(),1);
        QVERIFY(c.m_frameAssemblies.isEmpty());
    }
    void frameIdWrapAround() {
        AdminSessionController c; c.m_activeSessionId=42;
        QSignalSpy images(&c,&AdminSessionController::frameReady);
        auto b=jpeg();
        feed(c,42,0xffffffffu,0,1,b.size(),b);
        QTRY_COMPARE(images.count(),1);
        feed(c,42,0,0,1,b.size(),b);
        QTRY_COMPARE(images.count(),2);
        feed(c,42,0xffffffffu,0,1,b.size(),b);
        QTest::qWait(50);
        QCOMPARE(images.count(),2);
    }
    void newerFrameQueuedDuringDecodeSupersedesOlder() {
        AdminSessionController c; c.m_activeSessionId=42;
        QSignalSpy images(&c,&AdminSessionController::frameReady);
        auto red=jpeg(Qt::red), blue=jpeg(Qt::blue);
        feed(c,42,1,0,1,red.size(),red);
        QVERIFY(c.m_decoding);
        feed(c,42,2,0,1,blue.size(),blue);
        QTRY_COMPARE(images.count(),1);
        auto image=qvariant_cast<QImage>(images.at(0).at(1));
        QVERIFY(image.pixelColor(5,5).blue() > 180);
        QTest::qWait(50);
        QCOMPARE(images.count(),1);
    }
    void connectResultToImage() {
        AdminSessionController c; c.m_requestPending=true; c.m_pendingAgentSessionId=QStringLiteral("agent");
        QSignalSpy started(&c,&AdminSessionController::sessionEstablished); QSignalSpy images(&c,&AdminSessionController::frameReady);
        Protocol::ProtocolHeader h(Protocol::MessageType::CONNECT_RESULT); h.sessionId=42; h.payloadLength=1;
        auto b=jpeg();
        c.onRelayBytesReceived(Protocol::ProtocolSerializer::serializeHeader(h)+QByteArray(1,'\1')+packet(42,1,0,1,b.size(),b));
        QCOMPARE(started.count(),1); QTRY_COMPARE(images.count(),1);
    }
};
QTEST_MAIN(AdminScreenViewerTest)
#include "AdminScreenViewerTest.moc"
