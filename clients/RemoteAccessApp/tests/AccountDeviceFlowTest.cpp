#include <QtTest>
#include <QTcpServer>
#include <QScopeGuard>
#include <QSettings>
#include "Controllers/SettingsController.h"
#include "GUI/Pages/SettingsPage.h"
#include <QComboBox>
#include <QCheckBox>
#include <QApplication>
#include <QClipboard>
#include <QTemporaryFile>
#include "Controllers/AccountController.h"
#include "Network/Http/AccountService.h"
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
#include "GUI/Components/SidebarWidget.h"
#include "GUI/Pages/LogsPage.h"
#include "GUI/Components/DeviceCardWidget.h"
#include "Network/Protocol/RelayAuthPayload.h"
#include "Network/Http/HeartbeatReporter.h"
#include "Network/Http/ApiClient.h"
#include "Network/Relay/ChildSessionController.h"

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
    void emptyDevicesAreCenteredAndSidebarHasNoNewConnectionButton() {
        SidebarWidget sidebar;
        QVERIFY(!sidebar.findChild<QPushButton *>("newConnButton"));
        DevicesPage page;
        page.resize(900, 650);
        page.updateDeviceList({});
        page.show();
        QCoreApplication::processEvents();
        auto *title = page.findChild<QLabel *>("emptyStateTitle");
        QVERIFY(title && title->isVisible());
        const auto center = title->mapTo(&page, title->rect().center());
        QVERIFY(qAbs(center.x() - page.rect().center().x()) <= 2);
        auto *state = title->parentWidget();
        QVERIFY(qAbs(center.y() - state->mapTo(&page, state->rect().center()).y()) < 65);
        QSignalSpy refresh(&page, &DevicesPage::refreshRequested);
        page.findChild<QPushButton *>("emptyStateActionBtn")->click();
        QCOMPARE(refresh.count(), 1);
        const auto dir = qEnvironmentVariable("TEST_SCREENSHOT_DIR");
        if (!dir.isEmpty()) page.grab().save(dir + "/devices-centered.png");
    }
    void settingsExposeOnlyUsefulAccountControls() {
        SettingsPage page;
        const auto preview = qEnvironmentVariable("TEST_SCREENSHOT_DIR");
        if (!preview.isEmpty()) { page.resize(850, 600); page.show(); QCoreApplication::processEvents(); page.grab().save(preview + "/settings.png"); }
        QCOMPARE(page.findChildren<QLineEdit *>().size(), 3);
        QVERIFY(page.findChildren<QComboBox *>().isEmpty());
        QVERIFY(page.findChildren<QCheckBox *>().isEmpty());
        QCOMPARE(page.findChildren<QPushButton *>().size(), 1);
        for (auto *label : page.findChildren<QLabel *>()) {
            QVERIFY(!label->text().contains("Server API"));
            QVERIFY(!label->text().contains("JPEG"));
        }
    }
    void accountCardHasUsefulActionsAndCopiesFullLogin() {
        AccountCardWidget card(AccountInfo{1, "ownerchild", "child"});
        QCOMPARE(card.findChildren<QPushButton *>().size(), 3);
        auto *copy = card.findChild<QPushButton *>("copyAccountUsername");
        QVERIFY(copy);
        copy->click();
        QCOMPARE(QApplication::clipboard()->text(), QString("ownerchild"));
        for (auto *label : card.findChildren<QLabel *>())
            QVERIFY(!label->text().contains("MẬT KHẨU"));
    }
    void createShowsFullLoginAndBoundsSuffix() {
        CreateAccountDialog dialog;
        dialog.setOwnerUsername("owner");
        auto *name = dialog.findChild<QLineEdit *>("createChildUsername");
        auto *preview = dialog.findChild<QLabel *>("createLoginPreview");
        QVERIFY(name && preview);
        QCOMPARE(name->maxLength(), 45);
        name->setText(" child ");
        QCOMPARE(preview->text(), QString("Tên đăng nhập: ownerchild"));
    }
    void csvCreatesAccountsSequentially() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        auto &api = ApiClient::instance();
        const auto previous = api.getBaseUrl();
        const auto restore = qScopeGuard([&] { api.setBaseUrl(previous); });
        api.setBaseUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        QList<QPointer<QTcpSocket>> pending;
        int lists = 0;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                    const auto bytes = socket->property("buffer").toByteArray() + socket->readAll();
                    socket->setProperty("buffer", bytes);
                    if (!bytes.contains("\r\n\r\n") || socket->property("handled").toBool()) return;
                    socket->setProperty("handled", true);
                    if (bytes.startsWith("GET ")) {
                        ++lists;
                        const QByteArray body = R"({"success":true,"data":[]})";
                        socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                        socket->disconnectFromHost();
                    } else pending.append(socket);
                });
            }
        });
        QTemporaryFile csv;
        QVERIFY(csv.open());
        csv.write("username,password\na,example-password\nb,example-password\n");
        csv.flush();
        AccountStore store;
        AccountService service;
        AccountController controller(&store, &service);
        auto *search = controller.getView()->findChild<QLineEdit *>();
        QVERIFY(search);
        search->setText("stale-filter");
        QVERIFY(QMetaObject::invokeMethod(&controller, "handleRegisterRequested", Qt::DirectConnection,
            Q_ARG(QString, QString()), Q_ARG(QString, QString()), Q_ARG(QString, csv.fileName())));
        QTRY_COMPARE(pending.size(), 1);
        QTest::qWait(30);
        QCOMPARE(pending.size(), 1);
        auto reply = [&](int index) {
            QVERIFY(pending.at(index));
            pending.at(index)->write("HTTP/1.1 200 OK\r\nContent-Length: 16\r\nConnection: close\r\n\r\n{\"success\":true}");
            pending.at(index)->disconnectFromHost();
        };
        reply(0);
        QTRY_COMPARE(pending.size(), 2);
        reply(1);
        QTRY_COMPARE(lists, 1);
        QVERIFY(search->text().isEmpty());
    }
    void csvDialogPreservesSubmitLabelAndClearsErrorsOnPickOrClear() {
        CreateAccountDialog dialog;
        auto *save = dialog.findChild<QPushButton *>("createAccountSubmit");
        auto *clear = dialog.findChild<QPushButton *>("clearAccountCsv");
        QVERIFY(save && clear);
        dialog.selectImportFile("/tmp/empty.csv");
        QCOMPARE(save->text(), QString("Nhập danh sách"));
        dialog.showError("Không thể đọc file hoặc danh sách rỗng!");
        QCOMPARE(save->text(), QString("Nhập danh sách"));
        auto findError = [&]() -> QLabel * {
            for (auto *label : dialog.findChildren<QLabel *>()) {
                if (label->text() == "Không thể đọc file hoặc danh sách rỗng!") return label;
            }
            return nullptr;
        };
        auto *errorLabel = findError();
        QVERIFY(errorLabel && !errorLabel->isHidden());
        dialog.selectImportFile("/tmp/another.csv");
        QVERIFY(errorLabel->isHidden());
        dialog.showError("Không thể đọc file hoặc danh sách rỗng!");
        QVERIFY(!errorLabel->isHidden());
        clear->click();
        QCOMPARE(save->text(), QString("Thêm tài khoản"));
        QVERIFY(errorLabel->isHidden());
    }
    void settingsProfileFallbackAndPasswordValidation() {
        SettingsPage page;
        SettingsController controller(&page, nullptr);
        QVERIFY(QMetaObject::invokeMethod(&controller, "onProfileResult", Qt::DirectConnection,
            Q_ARG(bool, false), Q_ARG(QString, QString()), Q_ARG(QString, QString()), Q_ARG(QString, QStringLiteral("err"))));
        for (auto *label : page.findChildren<QLabel *>()) {
            QVERIFY(label->text() != QStringLiteral("Đang tải..."));
        }
        controller.setKnownUsername(QStringLiteral("admin"));
        QVERIFY(QMetaObject::invokeMethod(&controller, "onProfileResult", Qt::DirectConnection,
            Q_ARG(bool, false), Q_ARG(QString, QString()), Q_ARG(QString, QString()), Q_ARG(QString, QStringLiteral("err"))));
        bool sawAdmin = false;
        for (auto *label : page.findChildren<QLabel *>()) {
            if (label->text() == QStringLiteral("admin")) sawAdmin = true;
        }
        QVERIFY(sawAdmin);
        controller.handleChangePassword(QStringLiteral("oldpass"), QStringLiteral("      "), QStringLiteral("      "));
        bool sawBlankError = false;
        for (auto *label : page.findChildren<QLabel *>()) {
            if (label->text().contains(QStringLiteral("khoảng trắng"))) sawBlankError = true;
        }
        QVERIFY(sawBlankError);
        controller.handleChangePassword(QStringLiteral("oldpass"), QString(256, QLatin1Char('a')), QString(256, QLatin1Char('a')));
        bool sawLengthError = false;
        for (auto *label : page.findChildren<QLabel *>()) {
            if (label->text().contains(QStringLiteral("255"))) sawLengthError = true;
        }
        QVERIFY(sawLengthError);
    }
    void accountRefreshDuringFetchDiscardsOldSnapshotAndFetchesAgain() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        auto &api = ApiClient::instance();
        const auto previous = api.getBaseUrl();
        const auto restore = qScopeGuard([&] { api.setBaseUrl(previous); });
        api.setBaseUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        QList<QPointer<QTcpSocket>> peers;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                    const auto bytes = socket->property("buffer").toByteArray() + socket->readAll();
                    socket->setProperty("buffer", bytes);
                    if (!bytes.contains("\r\n\r\n") || socket->property("handled").toBool()) return;
                    socket->setProperty("handled", true);
                    peers.append(socket);
                });
            }
        });
        AccountService service;
        QSignalSpy results(&service, &AccountService::fetchListChildrenResult);
        service.fetchListChildren();
        QTRY_COMPARE(peers.size(), 1);
        service.fetchListChildren();
        auto respond = [&](int index, const QByteArray &body) {
            peers.at(index)->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
            peers.at(index)->disconnectFromHost();
        };
        respond(0, R"({"success":true,"data":[{"id":1,"username":"old","childUsername":"old"}]})");
        QTRY_COMPARE(peers.size(), 2);
        QCOMPARE(results.count(), 0);
        respond(1, R"({"success":true,"data":[{"id":1,"username":"new","childUsername":"new"}]})");
        QTRY_COMPARE(results.count(), 1);
        const auto accounts = qvariant_cast<QList<AccountInfo>>(results.first().at(1));
        QCOMPARE(accounts.size(), 1);
        QCOMPARE(accounts.first().username, QString("new"));
    }
    void logsShowOnlyRealCurrentRunEvents() {
        LogsPage page;
        for (auto *frame : page.findChildren<QFrame *>())
            QVERIFY(frame->property("role") != "logRow");
        bool emptyFound = false;
        for (auto *label : page.findChildren<QLabel *>())
            if (label->text().contains(QStringLiteral("Chưa có hoạt động"))) emptyFound = true;
        QVERIFY(emptyFound);
        page.addEntry(QStringLiteral("Bắt đầu phiên"), QStringLiteral("Laptop"));
        int rows = 0;
        for (auto *frame : page.findChildren<QFrame *>())
            if (frame->property("role") == "logRow") ++rows;
        QCOMPARE(rows, 1);
        for (auto *label : page.findChildren<QLabel *>())
            QVERIFY(!label->text().contains(QStringLiteral("j.doe@company.com")));
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
    void stopAndResumeSerializePresenceAfterInflightHeartbeat() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        auto &api = ApiClient::instance();
        const auto previousUrl = api.getBaseUrl();
        const auto restore = qScopeGuard([&] { api.setBaseUrl(previousUrl); });
        api.setBaseUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        QStringList routes;
        QList<QPointer<QTcpSocket>> peers;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                    const auto bytes = socket->property("buffer").toByteArray() + socket->readAll();
                    socket->setProperty("buffer", bytes);
                    if (!bytes.contains("\r\n\r\n") || socket->property("handled").toBool()) return;
                    socket->setProperty("handled", true);
                    routes.append(QString::fromUtf8(bytes.split(' ').value(1)));
                    peers.append(socket);
                });
            }
        });
        auto respond = [&](int index) {
            QVERIFY(peers.at(index));
            peers.at(index)->write("HTTP/1.1 200 OK\r\nContent-Length: 16\r\nConnection: close\r\n\r\n{\"success\":true}");
            peers.at(index)->disconnectFromHost();
        };
        HeartbeatReporter reporter;
        reporter.start();
        QTRY_COMPARE(routes.size(), 1);
        QCOMPARE(routes.at(0), QString("/api/v1/child/heartbeat"));
        reporter.stop();
        reporter.start();
        QTest::qWait(50);
        QCOMPARE(routes.size(), 1);
        respond(0);
        QTRY_COMPARE(routes.size(), 2);
        QCOMPARE(routes.at(1), QString("/api/v1/child/unshare"));
        respond(1);
        QTRY_COMPARE(routes.size(), 3);
        QCOMPARE(routes.at(2), QString("/api/v1/child/heartbeat"));
        reporter.stop();
        respond(2);
        QTRY_COMPARE(routes.size(), 4);
        QCOMPARE(routes.at(3), QString("/api/v1/child/unshare"));
        respond(3);
        QVERIFY(!reporter.isRunning());
    }
    void adminStartsWithAccountsAndReceivesDeviceUpdates() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        const auto previousUrl = ApiClient::instance().getBaseUrl();
        const auto restoreUrl = qScopeGuard([&] { ApiClient::instance().setBaseUrl(previousUrl); });
        ApiClient::instance().setBaseUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        int accountsRequests = 0, deviceRequests = 0, sseRequests = 0, updateRequests = 0, createRequests = 0, deleteRequests = 0;
        QPointer<QTcpSocket> ssePeer;
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
                    if (bytes.startsWith("GET /api/v1/devices/events ")) {
                        ++sseRequests;
                        ssePeer = socket;
                        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: keep-alive\r\n\r\n");
                        socket->flush();
                        return;
                    } else if (bytes.startsWith("GET /api/v1/child ")) {
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
                    } else if (bytes.startsWith("GET /api/v1/devices/events")) {
                        body = "";
                    } else if (bytes.startsWith("GET /api/v1/user/profile")) {
                        body = R"({"success":true,"data":{"username":"owner","email":"admin@example.com"}})";
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
        QTRY_COMPARE(deviceRequests, 1);
        QTRY_COMPARE(sseRequests, 1);
        QTRY_COMPARE(devices->getDevices().size(), 2);
        QVERIFY(ssePeer);
        ssePeer->write("event: device-list\ndata: []\n\n");
        ssePeer->flush();
        QTRY_VERIFY(devices->getDevices().isEmpty());
        ssePeer->write("event: device-list\ndata: [{\"sessionId\":\"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\",\"deviceName\":\"Laptop\"},{\"sessionId\":\"bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb\",\"deviceName\":\"PC\"}]\n\n");
        ssePeer->flush();
        QTRY_COMPARE(devices->getDevices().size(), 2);
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
        QTRY_COMPARE(deviceRequests, 2);
        QTRY_COMPARE(devices->getDevices().size(), 2);
        QTRY_COMPARE(devices->getDevices().first().username, QStringLiteral("ownerchild"));
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
        auto deleteAccountButton = [accountPage]() -> QPushButton * {
            for (auto *button : accountPage->findChildren<QPushButton*>("deleteAccountButton"))
                if (button->isVisible() && button->isEnabled()) return button;
            return nullptr;
        };
        QTRY_VERIFY(deleteAccountButton() != nullptr);
        QTimer::singleShot(0, []() {
            if (auto *confirmation = qobject_cast<QDialog*>(QApplication::activeModalWidget())) confirmation->accept();
        });
        deleteAccountButton()->click();
        QTRY_COMPARE(deleteRequests, 1);
        QTRY_VERIFY(accounts->getAccounts().isEmpty());
        QTRY_COMPARE(accountsRequests, 3);
        QVERIFY(QApplication::activeModalWidget() == nullptr);
        auto addAccountButton = [accountPage]() -> QPushButton * {
            for (auto *button : accountPage->findChildren<QPushButton*>()) {
                if (button->isVisible() && button->text() == "Thêm tài khoản") return button;
            }
            return nullptr;
        };
        QTRY_VERIFY(addAccountButton() != nullptr);
        addAccountButton()->click();
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

    void childCanSeeAndStopSharing() {
        const QByteArray oldHost = qgetenv("REMOTE_RELAY_HOST");
        const QByteArray oldPort = qgetenv("REMOTE_RELAY_PORT");
        qputenv("REMOTE_RELAY_HOST", "127.0.0.1");
        qputenv("REMOTE_RELAY_PORT", "1");
        {
            AppCoordinator coordinator;
            QVERIFY(QMetaObject::invokeMethod(&coordinator, "handleLoginSuccess", Qt::DirectConnection,
                    Q_ARG(QString, QStringLiteral("CHILD")), Q_ARG(QString, QStringLiteral("child"))));
            auto *window = [&]() -> QWidget * {
                for (auto *widget : QApplication::topLevelWidgets())
                    if (widget->objectName() == "childStatusWindow") return widget;
                return nullptr;
            }();
            QVERIFY(window);
            QVERIFY(window->isVisible());
            auto *status = window->findChild<QLabel *>("childSessionStatus");
            auto *stop = window->findChild<QPushButton *>("stopSharingButton");
            auto *heartbeat = coordinator.findChild<HeartbeatReporter *>();
            auto *session = coordinator.findChild<ChildSessionController *>();
            QVERIFY(status && stop && heartbeat && session);
            QVERIFY(!heartbeat->isRunning());
            QCOMPARE(stop->text(), QStringLiteral("Bật chia sẻ"));
            QVERIFY(status->text().contains(QStringLiteral("đang tắt")));
            const auto screenshotDir = qEnvironmentVariable("TEST_SCREENSHOT_DIR");
            if (!screenshotDir.isEmpty()) window->grab().save(screenshotDir + "/child-sharing-off.png");
            stop->click();
            QVERIFY(heartbeat->isRunning());
            QCOMPARE(stop->text(), QStringLiteral("Dừng chia sẻ"));
            emit session->relayRegistered();
            QVERIFY(status->text().contains(QStringLiteral("Sẵn sàng nhận phiên")));
            if (!screenshotDir.isEmpty()) window->grab().save(screenshotDir + "/child-sharing-on.png");
            stop->click();
            QVERIFY(status->text().contains(QStringLiteral("Đã dừng chia sẻ")));
            QCOMPARE(stop->text(), QStringLiteral("Bật chia sẻ"));
            QVERIFY(!heartbeat->isRunning());
            window->close();
        }
        if (oldHost.isNull()) qunsetenv("REMOTE_RELAY_HOST"); else qputenv("REMOTE_RELAY_HOST", oldHost);
        if (oldPort.isNull()) qunsetenv("REMOTE_RELAY_PORT"); else qputenv("REMOTE_RELAY_PORT", oldPort);
        QApplication::setQuitOnLastWindowClosed(true);
    }
};
QTEST_MAIN(AccountDeviceFlowTest)
#include "AccountDeviceFlowTest.moc"
