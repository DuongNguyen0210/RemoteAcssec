#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QDialog>
#include <QTimer>
#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTranslator>
#include <QMenu>
#include <QAction>
#include "GUI/Components/Accounts/CreateAccountDialog.h"
#include "GUI/Components/Accounts/AccountCardWidget.h"
#include "GUI/Components/Accounts/EditAccountDialog.h"
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
    void initTestCase() {
        auto *translation = new QTranslator(this);
        QVERIFY(translation->load(":/translations/qt_vi.qm"));
        QVERIFY(qApp->installTranslator(translation));
        QFile stylesheet(":/styles/Resources/styles/main.qss");
        QVERIFY(stylesheet.open(QIODevice::ReadOnly));
        qApp->setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    }
    void inputContextMenuIsVietnamese() {
        QLineEdit input;
        input.setText("abc");
        auto *menu = input.createStandardContextMenu();
        bool translatedCopy = false;
        for (auto *action : menu->actions()) {
            if (action->text().contains("Sao chép")) translatedCopy = true;
            QVERIFY(!action->text().contains("Copy"));
        }
        delete menu;
        QVERIFY(translatedCopy);
    }
    void accountPageStatesHaveOneAddActionAndSearchBothNames() {
        AccountPage page;
        page.resize(900, 650);
        page.show();
        auto visibleButtons = [&page](const QString &text) {
            QList<QPushButton*> result;
            for (auto *button : page.findChildren<QPushButton*>())
                if (button->isVisible() && button->text() == text) result.append(button);
            return result;
        };
        const auto dir = qEnvironmentVariable("TEST_SCREENSHOT_DIR");
        auto capture = [&](const QString &name) {
            QCoreApplication::processEvents();
            if (!dir.isEmpty()) page.grab().save(dir + "/" + name + ".png");
        };
        QTRY_COMPARE(visibleButtons("Thêm tài khoản").size(), 0);
        capture("accounts-loading");
        page.updateAccountList({});
        QTRY_COMPARE(visibleButtons("Thêm tài khoản").size(), 1);
        auto *search = page.findChild<QLineEdit*>("accountSearch");
        QVERIFY(!search->isVisible());
        QSignalSpy add(&page, &AccountPage::requestAddAccount);
        visibleButtons("Thêm tài khoản").first()->click();
        QCOMPARE(add.count(), 1);
        capture("accounts-empty");
        page.showLoadError("Kiểm tra mạng và thử lại.");
        QTRY_COMPARE(visibleButtons("Thêm tài khoản").size(), 0);
        QTRY_COMPARE(visibleButtons("Thử lại").size(), 1);
        QSignalSpy retry(&page, &AccountPage::loadRequested);
        visibleButtons("Thử lại").first()->click();
        QCOMPARE(retry.count(), 1);
        capture("accounts-error");
        page.updateAccountList({AccountInfo{1, "ownerchild", "child"}});
        QTRY_COMPARE(visibleButtons("Thêm tài khoản").size(), 1);
        QVERIFY(search->isVisible());
        search->setText("OWNER");
        QTRY_COMPARE(visibleButtons("Sửa").size(), 1);
        search->setText("missing");
        QTRY_COMPARE(visibleButtons("Sửa").size(), 0);
        QTRY_COMPARE(visibleButtons("Xóa tìm kiếm").size(), 1);
        capture("accounts-no-results");
        visibleButtons("Xóa tìm kiếm").first()->click();
        QTRY_COMPARE(visibleButtons("Sửa").size(), 1);
        page.showLoadError("Chưa cập nhật được. Dữ liệu có thể đã cũ.");
        QTRY_COMPARE(visibleButtons("Sửa").size(), 1);
        QTRY_COMPARE(visibleButtons("Thử lại").size(), 1);
        page.updateAccountList({});
        QTRY_COMPARE(visibleButtons("Thêm tài khoản").size(), 1);
    }
    void createAccountValidatesAndBlocksDuplicateSubmission() {
        CreateAccountDialog dialog;
        dialog.show();
        auto *name = dialog.findChild<QLineEdit*>("createChildUsername");
        auto *password = dialog.findChild<QLineEdit*>("createPassword");
        auto *submit = dialog.findChild<QPushButton*>("createAccountSubmit");
        QVERIFY(name && password && submit);
        QSignalSpy requests(&dialog, &CreateAccountDialog::registerRequested);
        submit->click();
        QCOMPARE(requests.count(), 0);
        name->setText(" child ");
        password->setText("example-password");
        submit->click();
        submit->click();
        QCOMPARE(requests.count(), 1);
        QCOMPARE(requests.first().first().toString(), QString("child"));
        QVERIFY(!submit->isEnabled());
        dialog.reject();
        QVERIFY(dialog.isVisible());
        dialog.showError("Tên tài khoản đã tồn tại.");
        QVERIFY(submit->isEnabled());
        const auto dir = qEnvironmentVariable("TEST_SCREENSHOT_DIR");
        if (!dir.isEmpty()) dialog.grab().save(dir + "/account-create.png");
        dialog.reject();
        QVERIFY(password->text().isEmpty());
    }
    void accountEditMasksNewPasswordAndEmitsChanges() {
        AccountInfo account{12, "ownerchild", "child"};
        AccountCardWidget card(account);
        QSignalSpy edit(&card, &AccountCardWidget::editRequested);
        QSignalSpy remove(&card, &AccountCardWidget::deleteRequested);
        for (auto *button : card.findChildren<QPushButton*>()) {
            if (button->objectName() == "editAccountButton" || button->objectName() == "deleteAccountButton") button->click();
        }
        QCOMPARE(edit.count(), 1);
        QCOMPARE(remove.count(), 1);
        QCOMPARE(edit.first().first().toString(), account.username);
        EditAccountDialog dialog(account);
        dialog.show();
        auto *password = dialog.findChild<QLineEdit*>("editNewPassword");
        auto *name = dialog.findChild<QLineEdit*>("editChildUsername");
        auto *toggle = dialog.findChild<QPushButton*>("toggleNewPassword");
        auto *save = dialog.findChild<QPushButton*>("saveAccountChanges");
        QVERIFY(password && name && toggle && save);
        QVERIFY(password->text().isEmpty());
        QCOMPARE(password->echoMode(), QLineEdit::Password);
        password->setText("example-new-password");
        QTest::mouseClick(toggle, Qt::LeftButton);
        QCOMPARE(password->echoMode(), QLineEdit::Normal);
        toggle->setFocus();
        QTest::keyClick(toggle, Qt::Key_Space);
        QCOMPARE(password->echoMode(), QLineEdit::Password);
        QSignalSpy changes(&dialog, &EditAccountDialog::saveRequested);
        name->setText(" ");
        save->click();
        QCOMPARE(changes.count(), 0);
        name->setText("renamed");
        password->clear();
        save->click();
        QCOMPARE(changes.count(), 1);
        QCOMPARE(changes.first().at(1).toString(), QString("renamed"));
        QVERIFY(changes.first().at(2).toString().isEmpty());
        QVERIFY(!save->isEnabled());
        dialog.showError("Thử lại.");
        QVERIFY(save->isEnabled());
        const auto screenshotDir = qEnvironmentVariable("TEST_SCREENSHOT_DIR");
        if (!screenshotDir.isEmpty()) {
            card.resize(826, card.sizeHint().height());
            card.show();
            card.grab().save(screenshotDir + "/account-card.png");
            dialog.grab().save(screenshotDir + "/account-edit.png");
        }
        password->setText("example-new-password");
        dialog.reject();
        QVERIFY(password->text().isEmpty());
    }
    void adminStartsWithAccountsAndLoadsDevicesOnlyOnDemand() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        qputenv("REMOTE_API_URL", QString("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        int accountsRequests = 0, deviceRequests = 0, updateRequests = 0, createRequests = 0, deleteRequests = 0;
        bool hasAccount = true;
        bool online = true;
        bool failDevices = false;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                    auto bytes = socket->property("buffer").toByteArray() + socket->readAll();
                    socket->setProperty("buffer", bytes);
                    if (!bytes.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
                    const int headerEnd = bytes.indexOf("\r\n\r\n") + 4;
                    int contentLength = 0;
                    for (const auto &line : bytes.left(headerEnd).split('\n')) {
                        if (line.toLower().startsWith("content-length:")) contentLength = line.mid(15).trimmed().toInt();
                    }
                    if (bytes.size() < headerEnd + contentLength) return;
                    socket->setProperty("answered", true);
                    QByteArray body;
                    if (bytes.startsWith("GET /api/v1/child ")) {
                        ++accountsRequests;
                        body = hasAccount ? R"({"success":true,"data":[{"id":12,"username":"ownerchild","childUsername":"child"}]})" : R"({"success":true,"data":[]})";
                    } else if (bytes.startsWith("DELETE /api/v1/child/ownerchild ")) {
                        ++deleteRequests;
                        hasAccount = false;
                        body = R"({"success":true})";
                    } else if (bytes.startsWith("POST /api/v1/child/Register ")) {
                        ++createRequests;
                        const auto request = QJsonDocument::fromJson(bytes.mid(headerEnd)).object();
                        QCOMPARE(request.value("childUsername").toString(), QString("child"));
                        QCOMPARE(request.value("password").toString(), QString("example-password"));
                        hasAccount = true;
                        body = R"({"success":true})";
                    } else if (bytes.startsWith("PUT /api/v1/child/12 ")) {
                        ++updateRequests;
                        body = R"({"success":true,"message":"Updated"})";
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
        if (!screenshotDir.isEmpty()) {
            window->grab().save(screenshotDir + "/accounts.png");
            for (int index : {1, 2, 3}) {
                stack->setCurrentIndex(index);
                QCoreApplication::processEvents();
                window->grab().save(screenshotDir + QString("/page-%1.png").arg(index));
            }
            stack->setCurrentIndex(4);
        }
        QPushButton *devicesButton = nullptr;
        for (auto *button : window->findChildren<QPushButton*>()) {
            if (button->property("pageIndex").isValid() && button->property("pageIndex").toInt() == 0) devicesButton = button;
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
        auto *accountPage = window->findChild<AccountPage*>();
        stack->setCurrentWidget(accountPage);
        auto accountCards = accountPage->findChildren<AccountCardWidget*>();
        QVERIFY(!accountCards.isEmpty());
        for (auto *button : accountCards.first()->findChildren<QPushButton*>()) {
            if (button->objectName() == "editAccountButton") button->click();
        }
        auto *editor = window->findChild<EditAccountDialog*>();
        QVERIFY(editor);
        editor->findChild<QLineEdit*>("editChildUsername")->setText("renamed");
        editor->findChild<QPushButton*>("saveAccountChanges")->click();
        QTRY_COMPARE(updateRequests, 1);
        QTRY_COMPARE(accountsRequests, 2);
        QTRY_VERIFY(window->findChild<EditAccountDialog*>() == nullptr);
        QTimer::singleShot(0, []() {
            if (auto *confirmation = qobject_cast<QDialog*>(QApplication::activeModalWidget())) confirmation->accept();
        });
        accountPage->findChild<QPushButton*>("deleteAccountButton")->click();
        QTRY_COMPARE(deleteRequests, 1);
        QTRY_VERIFY(accounts->getAccounts().isEmpty());
        QTRY_COMPARE(accountsRequests, 3);
        QVERIFY(QApplication::activeModalWidget() == nullptr);
        for (auto *button : accountPage->findChildren<QPushButton*>()) {
            if (button->isVisible() && button->text() == "Thêm tài khoản") { button->click(); break; }
        }
        auto *create = window->findChild<CreateAccountDialog*>();
        QVERIFY(create);
        create->findChild<QLineEdit*>("createChildUsername")->setText("child");
        create->findChild<QLineEdit*>("createPassword")->setText("example-password");
        create->findChild<QPushButton*>("createAccountSubmit")->click();
        QTRY_COMPARE(createRequests, 1);
        QTRY_COMPARE(accounts->getAccounts().size(), 1);
        QTRY_COMPARE(accountsRequests, 4);
        QTRY_VERIFY(window->findChild<CreateAccountDialog*>() == nullptr);

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
