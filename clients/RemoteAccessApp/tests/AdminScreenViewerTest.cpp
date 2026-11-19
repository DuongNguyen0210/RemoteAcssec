#include <QtTest>
#include <QTimer>
#include <QBuffer>
#include <QUrl>
#include <QScopeGuard>
#include <QtEndian>
#include "Network/Relay/AdminSessionController.h"
#include "Network/Protocol/ProtocolSerializer.h"
#include "GUI/Pages/SessionsPage.h"
#include "GUI/Pages/DevicesPage.h"
#include "GUI/Pages/AccountPage.h"
#include "GUI/Windows/MainWindow.h"
#include <QLineEdit>
#include <QLabel>
#include <QRandomGenerator>
#include <QStackedWidget>
#include <QPushButton>
#include "Network/Relay/ChildSessionController.h"
#include "Streaming/ScreenStreamSender.h"
#include "Streaming/ScreenEncoder.h"
#include "Controllers/AppCoordinator.h"
#include "Controllers/RemoteInputInjector.h"
#include "GUI/Windows/RemoteDesktopWindow.h"
#include "GUI/Components/RemoteScreenWidget.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include "Controllers/AuthController.h"
#include "Network/Http/ApiClient.h"
#include "Network/Protocol/ScreenFramePacketizer.h"
#include "Network/Relay/StaticRelayEndpointProvider.h"

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
    void loginWithForm(const QString &accountName, const QString &accountPassword, const QString &role) {
        AuthController auth;
        QSignalSpy login(&auth, &AuthController::loginSuccess);
        auth.start();
        QWidget *loginWindow = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (widget->objectName() == QStringLiteral("loginWindow") && widget->isVisible())
                loginWindow = widget;
        }
        QVERIFY(loginWindow);
        auto *username = loginWindow->findChild<QLineEdit *>("usernameInput");
        auto *password = loginWindow->findChild<QLineEdit *>("passwordInput");
        auto *submit = loginWindow->findChild<QPushButton *>("loginButton");
        QVERIFY(username && password && submit);
        username->setText(accountName);
        password->setText(accountPassword);
        QTest::mouseClick(submit, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(login.count(), 1, 8000);
        QCOMPARE(login.at(0).at(0).toString(), role);
        QCOMPARE(login.at(0).at(1).toString(), accountName);
        QVERIFY(!loginWindow->isVisible());
    }
    bool waitForReply(QNetworkReply *reply) {
        QSignalSpy finished(reply, &QNetworkReply::finished);
        return (reply->isFinished() || finished.wait(8000))
                && reply->error() == QNetworkReply::NoError;
    }
private slots:
    void boundsEncodedFramesWithoutDroppingLargeScreens() {
        QImage noisy(1600, 1200, QImage::Format_RGB32);
        QRandomGenerator random(42);
        for (int y = 0; y < noisy.height(); ++y) {
            auto *pixels = reinterpret_cast<QRgb *>(noisy.scanLine(y));
            for (int x = 0; x < noisy.width(); ++x) pixels[x] = 0xff000000u | random.generate();
        }
        const auto encoded = ScreenEncoder::encodeWithinLimit(noisy, 256 * 1024);
        QVERIFY(!encoded.jpeg.isEmpty());
        QVERIFY(encoded.jpeg.size() <= 256 * 1024);
        QVERIFY(encoded.size.width() < noisy.width());
        QCOMPARE(QImage::fromData(encoded.jpeg, "JPEG").size(), encoded.size);
    }
    void mapsFramePixelsToDesktopCoordinates() {
        const QRect desktop(100, 50, 1920, 1080);
        QCOMPARE(RemoteInputInjector::desktopPosition(1919, 1079, QSize(1920, 1080), desktop),
                 std::optional<QPoint>(QPoint(2019, 1129)));
        QCOMPARE(RemoteInputInjector::desktopPosition(1920, 1080, QSize(3840, 2160), desktop),
                 std::optional<QPoint>(QPoint(1060, 590)));
        QCOMPARE(RemoteInputInjector::desktopPosition(3839, 2159, QSize(3840, 2160), desktop),
                 std::optional<QPoint>(QPoint(2019, 1129)));
        QVERIFY(!RemoteInputInjector::desktopPosition(3840, 0, QSize(3840, 2160), desktop));
        QVERIFY(!RemoteInputInjector::desktopPosition(0, 0, QSize(), desktop));
    }
    void delayedInputRetainsViewedFrameGeometry() {
        const auto payload = RemoteInputPacket::withFrame(RemoteInputPacket::position(480, 270), QSize(960, 540));
        const auto reference = RemoteInputPacket::referenceFrame(payload, 8, QSize(1920, 1080));
        QCOMPARE(reference, QSize(960, 540));
        QCOMPARE(RemoteInputInjector::desktopPosition(480, 270, reference, QRect(0, 0, 1920, 1080)),
                 std::optional<QPoint>(QPoint(960, 540)));
        QVERIFY(RemoteInputPacket::referenceFrame(payload + 'x', 8, QSize(1920, 1080)).isEmpty());
        QVERIFY(RemoteInputPacket::referenceFrame(RemoteInputPacket::withFrame(RemoteInputPacket::position(0, 0), QSize(0, 0)), 8, QSize(1920, 1080)).isEmpty());
    }
    void viewerEncodesMouseAndKeyboardForRemoteSession() {
        RemoteScreenWidget view;
        view.resize(320, 240);
        QImage frame(160, 120, QImage::Format_RGB32); frame.fill(Qt::blue);
        view.updateFrame(frame);
        view.show();
        QVector<QPair<Protocol::MessageType, QByteArray>> events;
        connect(&view, &RemoteScreenWidget::remoteInput, &view,
                [&events](Protocol::MessageType type, const QByteArray &payload) {
                    events.append({type, payload});
                });
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(160, 120));
        QVERIFY(events.size() >= 2);
        QCOMPARE(events.at(0).first, Protocol::MessageType::MOUSE_BUTTON_DOWN);
        quint32 x = 0, y = 0;
        QVERIFY(RemoteInputPacket::readPosition(events.at(0).second, x, y));
        QCOMPARE(x, quint32(80)); QCOMPARE(y, quint32(60));
        QCOMPARE(RemoteInputPacket::referenceFrame(events.at(0).second, 9, QSize()), QSize(160, 120));
        QCOMPARE(events.at(1).first, Protocol::MessageType::MOUSE_BUTTON_UP);
        QTest::keyClick(&view, Qt::Key_A);
        QCOMPARE(events.constLast().first, Protocol::MessageType::KEY_RELEASE);
        QCOMPARE(events.constLast().second, RemoteInputPacket::key(Qt::Key_A));
        events.clear();
        QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, QPoint(160, 120));
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, QPoint(500, 500));
        QCOMPARE(events.size(), 2);
        QCOMPARE(events.at(0).first, Protocol::MessageType::MOUSE_BUTTON_DOWN);
        QCOMPARE(events.at(1).first, Protocol::MessageType::MOUSE_BUTTON_UP);
    }

    void viewerFullScreenPreservesSessionAndFrame() {
        RemoteDesktopWindow viewer(42, QStringLiteral("CHILD"));
        auto *toggle = viewer.findChild<QPushButton *>("remoteFullScreenButton");
        auto *screen = viewer.findChild<RemoteScreenWidget *>();
        QVERIFY(toggle && screen);
        QSignalSpy disconnects(&viewer, &RemoteDesktopWindow::disconnectRequested);
        QImage frame(32, 24, QImage::Format_RGB32);
        frame.fill(Qt::red);
        viewer.updateFrame(frame);
        viewer.show();

        toggle->click();
        QTRY_VERIFY(viewer.isFullScreen());
        QCOMPARE(toggle->text(), QStringLiteral("Thoát toàn màn hình"));
        toggle->click();
        QTRY_VERIFY(!viewer.isFullScreen());
        QVERIFY(!viewer.isMaximized());
        QCOMPARE(toggle->text(), QStringLiteral("Toàn màn hình"));

        viewer.showMaximized();
        toggle->click();
        QTRY_VERIFY(viewer.isFullScreen());
        toggle->click();
        QTRY_VERIFY(viewer.isMaximized() && !viewer.isFullScreen());
        QCOMPARE(screen->frameResolution(), frame.size());
        QCOMPARE(disconnects.count(), 0);
        viewer.close();
        QCOMPARE(disconnects.count(), 1);
    }

    void viewerReportsDelayedFramesAndRecovers() {
        RemoteDesktopWindow viewer(42, QStringLiteral("CHILD"));
        auto *status = viewer.findChild<QLabel *>("remoteFrameStatus");
        auto *watchdog = viewer.findChild<QTimer *>("frameWatchdog");
        auto *screen = viewer.findChild<RemoteScreenWidget *>();
        QVERIFY(status && watchdog && screen);
        QVERIFY(status->text().contains(QStringLiteral("Đang chờ hình ảnh")));
        QCOMPARE(watchdog->interval(), 5000);
        watchdog->setInterval(80);
        QTRY_VERIFY(status->text().contains(QStringLiteral("Chưa nhận được hình ảnh")));

        QImage frame(32, 24, QImage::Format_RGB32);
        frame.fill(Qt::white);
        viewer.updateFrame(frame);
        QVERIFY(status->text().contains(QStringLiteral("Đang truyền trực tiếp")));
        QTRY_VERIFY(status->text().contains(QStringLiteral("Hình ảnh chưa cập nhật")));
        QCOMPARE(screen->frameResolution(), frame.size());
        viewer.updateFrame({});
        QVERIFY(status->text().contains(QStringLiteral("Hình ảnh chưa cập nhật")));

        viewer.updateFrame(frame);
        QVERIFY(status->text().contains(QStringLiteral("Đang truyền trực tiếp")));
        viewer.closeForSessionEnd();
        QVERIFY(!watchdog->isActive());
    }

    void loopbackRelayDeliversSyntheticFrame() {
        if (!qEnvironmentVariableIsSet("PBL4_FRAME_SMOKE"))
            QSKIP("Requires an isolated Management API and Relay.");

        const auto adminUsername = qEnvironmentVariable("PBL4_SMOKE_ADMIN_USERNAME");
        const auto childUsername = qEnvironmentVariable("PBL4_SMOKE_CHILD_USERNAME");
        const auto password = qEnvironmentVariable("PBL4_SMOKE_PASSWORD");
        const QUrl apiUrl(qEnvironmentVariable("REMOTE_API_URL"));
        QVERIFY(apiUrl.host() == QStringLiteral("127.0.0.1") || apiUrl.host() == QStringLiteral("localhost"));
        QVERIFY(!adminUsername.isEmpty() && !childUsername.isEmpty() && !password.isEmpty());

        auto &api = ApiClient::instance();
        loginWithForm(childUsername, password, QStringLiteral("CHILD"));
        if (QTest::currentTestFailed())
            return;
        const auto childToken = api.getToken();
        QVERIFY(!childToken.isEmpty());
        const auto logout = qScopeGuard([&] {
            api.setToken(childToken);
            auto *reply = api.post("/api/v1/auth/logout", {});
            waitForReply(reply);
            reply->deleteLater();
            api.setToken({});
        });
        auto *heartbeat = api.post("/api/v1/child/heartbeat",
                                   {{"hostname", "Synthetic fixture"}, {"os", "Windows"}});
        const bool heartbeatSent = waitForReply(heartbeat);
        heartbeat->deleteLater();
        QVERIFY(heartbeatSent);
        auto *identity = api.get("/api/v1/relay/agent");
        const bool identified = waitForReply(identity);
        const auto agentId = QString::fromUtf8(identity->readAll()).trimmed();
        identity->deleteLater();
        QVERIFY(identified && !agentId.isEmpty());

        auto *allocation = api.post("/api/v1/relay/allocations", {});
        const bool allocated = waitForReply(allocation);
        const auto relay = ApiClient::parseReply(allocation).data.toObject();
        allocation->deleteLater();
        QVERIFY(allocated);
        const auto host = relay.value("host").toString();
        QVERIFY(host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1"));
        const int port = relay.value("port").toInt();
        QVERIFY(port > 0 && port <= 65535);
        qputenv("REMOTE_RELAY_HOST", host.toUtf8());
        qputenv("REMOTE_RELAY_PORT", QByteArray::number(port));
        StaticRelayEndpointProvider provider;
        ChildSessionController child(&provider);
        if (qEnvironmentVariable("REMOTE_RELAY_MODE") == "dynamic") {
            qunsetenv("REMOTE_RELAY_HOST");
            qunsetenv("REMOTE_RELAY_PORT");
        }
        AdminSessionController admin;
        QSignalSpy childStarted(&child, &ChildSessionController::sessionStarted);
        QSignalSpy childEnded(&child, &ChildSessionController::sessionEnded);
        QSignalSpy adminStarted(&admin, &AdminSessionController::sessionEstablished);
        QSignalSpy frames(&admin, &AdminSessionController::frameReady);
        ApiClient::instance().setToken(childToken);
        child.start();
        QTRY_VERIFY_WITH_TIMEOUT(child.relayReady(), 8000);
        ApiClient::instance().setToken({});
        loginWithForm(adminUsername, password, QStringLiteral("ADMIN"));
        if (QTest::currentTestFailed())
            return;
        admin.requestSession(agentId);
        QTRY_COMPARE_WITH_TIMEOUT(adminStarted.count(), 1, 8000);
        QCOMPARE(childStarted.count(), 1);
        const quint64 id = child.activeSessionId();
        QVERIFY(id != 0);
        QCOMPARE(adminStarted.at(0).at(0).toULongLong(), id);

        RemoteDesktopWindow viewer(id, QStringLiteral("Synthetic CHILD"));
        auto *screen = viewer.findChild<RemoteScreenWidget *>();
        QVERIFY(screen);
        connect(&admin, &AdminSessionController::frameReady, &viewer,
                [&viewer, id](quint64 session, const QImage &image) {
            if (session == id)
                viewer.updateFrame(image);
        });
        const auto packets = ScreenFramePacketizer::packetize(jpeg(), 1, id);
        QVERIFY(!packets.isEmpty());
        for (const auto &bytes : packets)
            QCOMPARE(child.sendScreenPacket(bytes), qint64(bytes.size()));
        QTRY_COMPARE(frames.count(), 1);
        QCOMPARE(screen->frameResolution(), QSize(32, 24));
        const auto image = qvariant_cast<QImage>(frames.at(0).at(1));
        QVERIFY(image.pixelColor(5, 5).red() > 180);
        admin.disconnectSession();
        QTRY_COMPARE(childEnded.count(), 1);
        QCOMPARE(child.activeSessionId(), quint64(0));
        child.stop();
    }
    void remoteDesktopCloseIsIdempotentAndProgrammaticCloseIsSilent() {
        RemoteDesktopWindow userWindow(42, QStringLiteral("child"));
        userWindow.show();
        QSignalSpy requested(&userWindow, &RemoteDesktopWindow::disconnectRequested);
        connect(&userWindow, &RemoteDesktopWindow::disconnectRequested,
                &userWindow, &RemoteDesktopWindow::closeForSessionEnd);
        auto *button = userWindow.findChild<QPushButton *>("remoteDisconnectButton"); QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(requested.count(), 1);
        userWindow.closeForSessionEnd();
        QCOMPARE(requested.count(), 1);
        RemoteDesktopWindow endedWindow(43, QStringLiteral("child"));
        endedWindow.show();
        QSignalSpy endedRequests(&endedWindow, &RemoteDesktopWindow::disconnectRequested);
        endedWindow.closeForSessionEnd();
        QCOMPARE(endedRequests.count(), 0);
    }
    void coordinatorRendersOnlyCurrentSessionAndClosesOnEnd() {
        AppCoordinator coordinator;
        QVERIFY(QMetaObject::invokeMethod(&coordinator, "handleRemoteSessionStarted", Qt::DirectConnection,
                Q_ARG(quint64, 42), Q_ARG(QString, QStringLiteral("child"))));
        RemoteDesktopWindow *window = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *candidate = qobject_cast<RemoteDesktopWindow *>(widget);
            if (candidate && candidate->isVisible()) window = candidate;
        }
        QVERIFY(window);
        auto *screen = window->findChild<RemoteScreenWidget *>(); QVERIFY(screen);
        QImage frame(32,24,QImage::Format_RGB32); frame.fill(Qt::red);
        QVERIFY(QMetaObject::invokeMethod(&coordinator, "handleFrameReady", Qt::DirectConnection,
                Q_ARG(quint64, 41), Q_ARG(QImage, frame)));
        QVERIFY(screen->frameResolution().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(&coordinator, "handleFrameReady", Qt::DirectConnection,
                Q_ARG(quint64, 42), Q_ARG(QImage, frame)));
        QCOMPARE(screen->frameResolution(), frame.size());
        QVERIFY(QMetaObject::invokeMethod(&coordinator, "handleSessionEnded", Qt::DirectConnection,
                Q_ARG(quint64, 41), Q_ARG(QString, QString()), Q_ARG(QString, QString())));
        QVERIFY(window->isVisible());
        QSignalSpy requested(window, &RemoteDesktopWindow::disconnectRequested);
        QVERIFY(QMetaObject::invokeMethod(&coordinator, "handleSessionEnded", Qt::DirectConnection,
                Q_ARG(quint64, 42), Q_ARG(QString, QString()), Q_ARG(QString, QString())));
        QVERIFY(!window->isVisible());
        QCOMPARE(requested.count(), 0);
    }
    void childSessionSignalsStartAndStopStreaming() {
        ChildSessionController child(nullptr);
        ScreenStreamSender sender(&child);
        auto *timer = sender.findChild<QTimer *>(); QVERIFY(timer);
        QVERIFY(!timer->isActive());
        emit child.sessionStarted(42);
        QVERIFY(timer->isActive());
        emit child.sessionEnded(QStringLiteral("test"));
        QVERIFY(!timer->isActive());
        emit child.sessionStarted(43);
        QVERIFY(timer->isActive());
        emit child.sessionEnded(QStringLiteral("test"));
        QVERIFY(!timer->isActive());
    }
    void sessionNavigationKeepsSidebarAndTopbarInSync() {
        auto *devicePage = new DevicesPage;
        MainWindow window(devicePage, new AccountPage);
        devicePage->updateDeviceList({
            {"session-a", 1, "alice", "DESKTOP-A", "Windows", "10.0.0.1", 0},
            {"session-b", 2, "bob", "DESKTOP-B", "Linux", "10.0.0.2", 0}
        });
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
        QVERIFY(search->isEnabled());
        QCOMPARE(search->placeholderText(), QStringLiteral("Tìm thiết bị"));
        QCOMPARE(changed.count(), 1);
        search->setText("alice");
        QCOMPARE(stack->currentIndex(), 0);
        QVERIFY(devices->isChecked());
        QCOMPARE(changed.count(), 2);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(devicePage->findChildren<QWidget *>("deviceCard").size(), 1);
        search->setText("alice-x");
        QCOMPARE(changed.count(), 2);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(devicePage->findChildren<QWidget *>("deviceCard").size(), 0);
        auto *clear = devicePage->findChild<QPushButton *>("emptyStateActionBtn");
        QVERIFY(clear);
        QTest::mouseClick(clear, Qt::LeftButton);
        QCOMPARE(search->text(), QString());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(devicePage->findChildren<QWidget *>("deviceCard").size(), 2);
        window.showRemoteSession();
        QCOMPARE(stack->currentIndex(), 1);
        QVERIFY(sessions->isChecked());
        QCOMPARE(search->placeholderText(), QStringLiteral("Tìm thiết bị"));
    }
    void disconnectButtonClearsViewer() {
        SessionsPage page; page.resize(900,700); page.show();
        auto *empty = page.findChild<QLabel *>("sessionsEmptyState"); QVERIFY(empty);
        QVERIFY(empty->isVisible());
        page.showSession();
        QVERIFY(!empty->isVisible());
        auto *button = page.findChild<QPushButton *>("disconnectSessionButton"); QVERIFY(button);
        auto *viewer = page.findChild<QWidget *>("screenViewer"); QVERIFY(viewer);
        QVERIFY(viewer->isVisible());
        QImage image(32,24,QImage::Format_RGB32); image.fill(Qt::red); page.setFrame(image);
        QSignalSpy requested(&page, &SessionsPage::disconnectRequested);
        connect(&page, &SessionsPage::disconnectRequested, &page, &SessionsPage::clearFrame);
        auto before = viewer->grab().toImage();
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(requested.count(),1); QVERIFY(viewer->grab().toImage() != before);
        page.clearFrame();
        QVERIFY(empty->isVisible());
        QVERIFY(!viewer->isVisible());
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
        for (auto *widget : page.findChildren<QWidget *>())
            QVERIFY(widget->property("role") != "metricCard");
        QCOMPARE(page.findChildren<QPushButton *>().size(), 1);
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
        QCOMPARE(spy.count(),0); QCOMPARE(c.m_frameReassembler.m_frames[1].receivedChunks,quint32(1));
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
        QTest::qWait(50); QCOMPARE(spy.count(),0); QVERIFY(!c.m_frameReassembler.m_frames.contains(3));
        QVERIFY(!c.m_frameReassembler.m_frames.contains(4));
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
        QCOMPARE(c.m_frameReassembler.m_frames.size(),2); QCOMPARE(c.m_frameReassembler.m_reservedBytes,quint64(16*1024*1024));
        for (auto it=c.m_frameReassembler.m_frames.begin();it!=c.m_frameReassembler.m_frames.end();++it) it->createdAt=-6000;
        c.pruneFrames(); QCOMPARE(c.m_frameReassembler.m_frames.size(),0); QCOMPARE(c.m_frameReassembler.m_reservedBytes,quint64(0));
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
        QVERIFY(c.m_frameReassembler.m_frames.isEmpty());
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
    void decoderMakesProgressAndKeepsOnlyLatestPendingFrame() {
        AdminSessionController c; c.m_activeSessionId=42;
        QSignalSpy images(&c,&AdminSessionController::frameReady);
        auto red=jpeg(Qt::red), blue=jpeg(Qt::blue), green=jpeg(Qt::green);
        feed(c,42,1,0,1,red.size(),red);
        QVERIFY(c.m_decoding);
        // Arrival outpaces decode: finish the current image, then decode only the latest.
        feed(c,42,2,0,1,blue.size(),blue);
        feed(c,42,3,0,1,green.size(),green);
        QTRY_COMPARE(images.count(),2);
        auto first=qvariant_cast<QImage>(images.at(0).at(1));
        auto last=qvariant_cast<QImage>(images.at(1).at(1));
        QVERIFY(first.pixelColor(5,5).red() > 180);
        QVERIFY(last.pixelColor(5,5).green() > 180);
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
