#include <QtTest>
#include <QEventLoop>
#include <QLineEdit>
#include <QPushButton>
#include <QUuid>
#include "Controllers/AppCoordinator.h"
#include "GUI/Windows/MainWindow.h"
#include "Network/Http/ApiClient.h"

// Run only against a disposable loopback API; never uses existing user accounts.
class SingleLoginDesktopAcceptance : public QObject {
    Q_OBJECT
    ApiParsedResponse post(const QString &path, const QJsonObject &body = {}) {
        auto *reply = ApiClient::instance().post("/api/v1/" + path, body);
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        if (!reply->isFinished()) loop.exec();
        auto result = ApiClient::parseReply(reply);
        reply->deleteLater();
        return result;
    }
    QWidget *window(const QString &name) {
        for (auto *widget : QApplication::topLevelWidgets())
            if (widget->objectName() == name && widget->isVisible()) return widget;
        return nullptr;
    }
private slots:
    void loginThroughEnterMaintainsAndReleasesLease() {
        if (qEnvironmentVariable("TEST_DISPOSABLE_ACCOUNT_STACK") != "1")
            QSKIP("Requires a disposable API stack");
        const QUrl url(qEnvironmentVariable("REMOTE_API_URL"));
        QVERIFY(url.host() == "127.0.0.1" || url.host() == "localhost");
        QGuiApplication::setQuitOnLastWindowClosed(false);
        ApiClient::instance().setBaseUrl(url.toString());
        const auto owner = "desktop" + QUuid::createUuid().toString(QUuid::Id128).left(10);
        const auto password = QUuid::createUuid().toString(QUuid::Id128);
        QVERIFY(post("auth/register", {{"username", owner}, {"password", password}}).success);
        auto login = post("auth/login", {{"username", owner}, {"password", password}});
        QVERIFY(login.success);
        ApiClient::instance().setToken(login.data.toObject()["token"].toString());
        QVERIFY(post("child/register", {{"childUsername", "child"}, {"password", password}}).success);
        QVERIFY(post("auth/logout").success);
        for (const auto &username : {owner, owner + "child"}) {
            ApiClient::instance().setToken({});
            {
                AppCoordinator coordinator;
                coordinator.start();
                auto *form = window("loginWindow"); QVERIFY(form);
                auto *user = form->findChild<QLineEdit *>("usernameInput"); QVERIFY(user);
                auto *pass = form->findChild<QLineEdit *>("passwordInput"); QVERIFY(pass);
                user->setText(username); pass->setText(password);
                QTest::keyClick(pass, Qt::Key_Return);
                QTRY_VERIFY_WITH_TIMEOUT(!ApiClient::instance().getToken().isEmpty(), 15000);
                QTRY_VERIFY_WITH_TIMEOUT(!form->isVisible(), 15000);
                auto duplicate = post("auth/login", {{"username", username}, {"password", password}});
                QCOMPARE(duplicate.httpStatusCode, 409);
                QCOMPARE(duplicate.errorCode, QStringLiteral("ACCOUNT_IN_USE"));
                // Real timer renews independently of sharing (CHILD stays off).
                int renewals = 0;
                const auto connection = connect(ApiClient::instance().getManager(), &QNetworkAccessManager::finished,
                        this, [&](QNetworkReply *reply) {
                    if (reply->url().path() == "/api/v1/auth/heartbeat"
                            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) ++renewals;
                });
                QTRY_VERIFY_WITH_TIMEOUT(renewals >= 1, 7000);
                disconnect(connection);
                duplicate = post("auth/login", {{"username", username}, {"password", password}});
                QCOMPARE(duplicate.httpStatusCode, 409);
            } // Real coordinator shuts down and synchronously releases its lease.
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            login = post("auth/login", {{"username", username}, {"password", password}});
            QVERIFY(login.success);
            ApiClient::instance().setToken(login.data.toObject()["token"].toString());
            QVERIFY(post("auth/logout").success);
        }
        ApiClient::instance().setToken({});
    }
};
QTEST_MAIN(SingleLoginDesktopAcceptance)
#include "SingleLoginDesktopAcceptance.moc"
