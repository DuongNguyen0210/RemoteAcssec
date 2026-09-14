#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QDialog>
#include <QTimer>
#include "Controllers/AppCoordinator.h"
#include "Domain/Store/AccountStore.h"
#include "Domain/Store/DeviceStore.h"
#include "GUI/Windows/MainWindow.h"
#include "GUI/Pages/AccountPage.h"
#include "GUI/Pages/DevicesPage.h"
#include "GUI/Components/DeviceCardWidget.h"
#include "Network/Protocol/RelayAuthPayload.h"

class AccountDeviceFlowTest : public QObject {
    Q_OBJECT
private slots:
    void adminStartsWithAccountsAndLoadsDevicesOnlyOnDemand() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        qputenv("REMOTE_API_URL", QString("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        int accountsRequests = 0, deviceRequests = 0;
        bool online = true;
        bool failDevices = false;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                    auto bytes = socket->property("buffer").toByteArray() + socket->readAll();
                    socket->setProperty("buffer", bytes);
                    if (!bytes.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
                    socket->setProperty("answered", true);
                    QByteArray body;
                    if (bytes.startsWith("GET /api/v1/child ")) {
                        ++accountsRequests;
                        body = R"({"success":true,"data":[{"id":12,"username":"ownerchild","childUsername":"child"}]})";
                    } else if (bytes.startsWith("GET /api/v1/devices ")) {
                        ++deviceRequests;
                        body = online ? R"({"success":true,"data":[{"sessionId":"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa","childId":12,"username":"ownerchild","deviceName":"Laptop","os":"Linux","ipAddress":"192.168.1.10"},{"sessionId":"bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb","childId":12,"username":"ownerchild","deviceName":"PC","os":"Windows","ipAddress":"192.168.1.11"}]})"
                                      : R"({"success":true,"data":[]})";
                    } else { QFAIL("Unexpected API route"); }
                    bool failed = failDevices && bytes.startsWith("GET /api/v1/devices ");
                    if (failed) body = R"({"success":false,"message":"Redis unavailable"})";
                    QByteArray status = failed ? "HTTP/1.1 503 Service Unavailable" : "HTTP/1.1 200 OK";
                    socket->write(status + "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
        QString styles;
        for (const auto &name : {"main", "sidebar", "topbar", "devicecard"}) {
            QFile f(QString(":/styles/Resources/styles/%1.qss").arg(name));
            if (f.open(QIODevice::ReadOnly)) styles += QString::fromUtf8(f.readAll());
        }
        qApp->setStyleSheet(styles);
        AppCoordinator coordinator;
        QVERIFY(QMetaObject::invokeMethod(&coordinator, "handleLoginSuccess", Qt::DirectConnection,
                Q_ARG(QString, "ADMIN"), Q_ARG(QString, "owner")));
        auto *accounts = coordinator.findChild<AccountStore*>();
        auto *devices = coordinator.findChild<DeviceStore*>();
        QVERIFY(accounts && devices);
        QTRY_COMPARE(accounts->getAccounts().size(), 1);
        QCOMPARE(accountsRequests, 1);
        QCOMPARE(deviceRequests, 0);
        QVERIFY(devices->getDevices().isEmpty());
        MainWindow *window = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (auto *candidate = qobject_cast<MainWindow*>(widget)) window = candidate;
        }
        QVERIFY(window);
        auto *stack = window->findChild<QStackedWidget*>();
        QVERIFY(qobject_cast<AccountPage*>(stack->currentWidget()));
        const auto screenshotDir = qEnvironmentVariable("TEST_SCREENSHOT_DIR");
        if (!screenshotDir.isEmpty()) window->grab().save(screenshotDir + "/accounts.png");
        QPushButton *devicesButton = nullptr;
        for (auto *button : window->findChildren<QPushButton*>()) {
            if (button->text() == "Devices") devicesButton = button;
        }
        QVERIFY(devicesButton);
        QTest::mouseClick(devicesButton, Qt::LeftButton);
        QTRY_COMPARE(devices->getDevices().size(), 2);
        QCOMPARE(accounts->getAccounts().size(), 1);
        QCOMPARE(accountsRequests, 1);
        auto *page = window->findChild<DevicesPage*>();
        stack->setCurrentWidget(page);
        auto cards = page->findChildren<DeviceCardWidget*>();
        QCOMPARE(cards.size(), 2);
        QObject::disconnect(page, SIGNAL(connectRequested(QString)), nullptr, nullptr);
        QSignalSpy selected(page, &DevicesPage::connectRequested);
        QVERIFY(QMetaObject::invokeMethod(cards.first(), "connectRequested", Qt::DirectConnection,
                Q_ARG(QString, devices->getDevices().first().sessionId)));
        QCOMPARE(selected.first().first().toString(), devices->getDevices().first().sessionId);
        QVERIFY(page->findChildren<QPushButton*>("removeDeviceButton").isEmpty());
        if (!screenshotDir.isEmpty()) window->grab().save(screenshotDir + "/devices.png");
        // A failed refresh keeps both cached devices visible and reports stale data.
        failDevices = true;
        bool errorShown = false;
        QTimer dismissError;
        connect(&dismissError, &QTimer::timeout, this, [&]() {
            if (auto *dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                errorShown = true;
                dialog->accept();
            }
        });
        dismissError.start(10);
        QVERIFY(QMetaObject::invokeMethod(&coordinator, "handlePageSelected", Qt::DirectConnection, Q_ARG(int, 0)));
        QTRY_VERIFY(errorShown);
        dismissError.stop();
        QCOMPARE(devices->getDevices().size(), 2);
        QCOMPARE(page->findChildren<DeviceCardWidget*>().size(), 2);
        failDevices = false;
        // Expiry removes only device rows after refresh, never the account.
        online = false;
        QVERIFY(QMetaObject::invokeMethod(&coordinator, "handlePageSelected", Qt::DirectConnection, Q_ARG(int, 0)));
        QTRY_VERIFY(devices->getDevices().isEmpty());
        QCOMPARE(accounts->getAccounts().size(), 1);
        QCOMPARE(accountsRequests, 1);
    }

    void relayPayloadCarriesTokenAndAgentSessionSeparately() {
        auto payload = Protocol::relayAuthPayload("abc", "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
        QCOMPARE(payload.left(5), QByteArray::fromHex("0003616263"));
        QCOMPARE(payload.mid(5, 2), QByteArray::fromHex("0024"));
        QVERIFY(Protocol::relayAuthPayload("").isEmpty());
        QVERIFY(Protocol::relayAuthPayload(QString(4097, 'x')).isEmpty());
    }
};
QTEST_MAIN(AccountDeviceFlowTest)
#include "AccountDeviceFlowTest.moc"
