#include <QtTest>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QPointer>
#include <cstdio>
#include "Controllers/AppCoordinator.h"
#include "GUI/Windows/MainWindow.h"
#include "GUI/Windows/RemoteDesktopWindow.h"
#include "GUI/Components/RemoteScreenWidget.h"
#include "GUI/Components/SidebarWidget.h"
#include "Network/Relay/ChildSessionController.h"
#include "Network/Relay/AdminSessionController.h"

template<class T> T *topWindow(const QString &name = {}) {
    for (auto *widget : QApplication::topLevelWidgets())
        if (auto *candidate = qobject_cast<T *>(widget))
            if (name.isEmpty() || candidate->objectName() == name) return candidate;
    return nullptr;
}

class FullUiDesktopAcceptance : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { std::setvbuf(stdout, nullptr, _IONBF, 0); }
    void loginShareControlAndStop() {
        const auto role = qEnvironmentVariable("PBL4_DESKTOP_ROLE");
        QVERIFY(role == "child" || role == "admin");
        QVERIFY(!qEnvironmentVariable("PBL4_UI_USERNAME").isEmpty());
        AppCoordinator coordinator;
        coordinator.start();
        auto *login = topWindow<QWidget>("loginWindow"); QVERIFY(login);
        auto *username = login->findChild<QLineEdit *>("usernameInput"); QVERIFY(username);
        auto *password = login->findChild<QLineEdit *>("passwordInput"); QVERIFY(password);
        auto *submit = login->findChild<QPushButton *>("loginButton"); QVERIFY(submit);
        username->setText(qEnvironmentVariable("PBL4_UI_USERNAME"));
        password->setText(qEnvironmentVariable("PBL4_UI_PASSWORD"));
        QTest::mouseClick(submit, Qt::LeftButton);
        if (role == "child") {
            QVERIFY(qEnvironmentVariable("PBL4_ISOLATED_X11") == "1");
            QTRY_VERIFY_WITH_TIMEOUT(topWindow<QWidget>("childStatusWindow"), 20000);
            auto *statusWindow = topWindow<QWidget>("childStatusWindow");
            auto *sharing = statusWindow->findChild<QPushButton *>("stopSharingButton"); QVERIFY(sharing);
            auto *session = coordinator.findChild<ChildSessionController *>(); QVERIFY(session);
            QCOMPARE(sharing->text(), QStringLiteral("Bật chia sẻ"));
            QVERIFY(!session->relayReady());
            QTest::mouseClick(sharing, Qt::LeftButton);
            QTRY_VERIFY_WITH_TIMEOUT(session->relayReady(), 20000);
            qInfo() << "CHILD_UI_READY";
            QWidget marker;
            marker.setGeometry(100, 100, 600, 400);
            marker.setAutoFillBackground(true);
            auto color = [&](const QColor &value) {
                auto palette = marker.palette(); palette.setColor(QPalette::Window, value); marker.setPalette(palette);
            };
            color(Qt::magenta);
            auto *button = new QPushButton("Native click", &marker); button->setGeometry(50, 50, 200, 80);
            auto *edit = new QLineEdit(&marker); edit->setGeometry(50, 160, 300, 60);
            connect(button, &QPushButton::clicked, &marker, [&] { color(Qt::green); });
            bool typed = false, stopped = false;
            connect(edit, &QLineEdit::textChanged, &marker, [&](const QString &text) {
                if (text != "a" || typed) return;
                typed = true; color(Qt::blue);
                QTimer::singleShot(2000, &marker, [&] {
                    QTest::mouseClick(sharing, Qt::LeftButton); stopped = true;
                });
            });
            marker.show(); marker.raise(); marker.activateWindow();
            QTRY_VERIFY_WITH_TIMEOUT(typed, 30000);
            QTRY_VERIFY_WITH_TIMEOUT(stopped, 10000);
            QVERIFY(!session->relayReady());
            QCOMPARE(sharing->text(), QStringLiteral("Bật chia sẻ"));
            qInfo() << "CHILD_LOCAL_STOP_CONFIRMED";
        } else {
            QTRY_VERIFY_WITH_TIMEOUT(topWindow<MainWindow>(), 20000);
            auto *main = topWindow<MainWindow>();
            main->findChild<SidebarWidget *>()->selectPage(0);
            QTRY_VERIFY_WITH_TIMEOUT(main->findChild<QPushButton *>("connectButton"), 20000);
            auto *session = coordinator.findChild<AdminSessionController *>(); QVERIFY(session);
            QImage latest;
            QObject observer;
            connect(session, &AdminSessionController::frameReady, &observer,
                    [&](quint64, const QImage &image) { latest = image; });
            QSignalSpy ended(session, &AdminSessionController::sessionEnded);
            QTest::mouseClick(main->findChild<QPushButton *>("connectButton"), Qt::LeftButton);
            QTRY_VERIFY_WITH_TIMEOUT(topWindow<RemoteDesktopWindow>(), 20000);
            QPointer<RemoteDesktopWindow> remote = topWindow<RemoteDesktopWindow>();
            auto *viewer = remote->findChild<RemoteScreenWidget *>(); QVERIFY(viewer);
            auto isColor = [&](int red, int green, int blue) {
                if (latest.size() != QSize(1280, 800)) return false;
                const auto pixel = latest.pixelColor(102, 102);
                return qAbs(pixel.red()-red)<60 && qAbs(pixel.green()-green)<60 && qAbs(pixel.blue()-blue)<60;
            };
            auto clickRemote = [&](const QPoint &pixel) {
                const QSize drawn = latest.size().scaled(viewer->size(), Qt::KeepAspectRatio);
                const QPoint origin((viewer->width()-drawn.width())/2, (viewer->height()-drawn.height())/2);
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                                  origin + QPoint(pixel.x()*drawn.width()/latest.width(), pixel.y()*drawn.height()/latest.height()));
            };
            QTRY_VERIFY_WITH_TIMEOUT(isColor(255,0,255), 15000);
            clickRemote(QPoint(250,190));
            QTRY_VERIFY_WITH_TIMEOUT(isColor(0,255,0), 10000);
            clickRemote(QPoint(250,290));
            QTest::keyClick(viewer, Qt::Key_A);
            QTRY_VERIFY_WITH_TIMEOUT(isColor(0,0,255), 10000);
            qInfo() << "ADMIN_UI_INPUT_CONFIRMED";
            QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 15000);
            QTRY_VERIFY(!remote || !remote->isVisible());
            QVERIFY(main->isVisible());
            QVERIFY(!QApplication::activeModalWidget());
            auto *notice = main->findChild<QLabel *>("deviceSessionStatus"); QVERIFY(notice);
            QVERIFY(!notice->text().isEmpty());
            auto *refresh = main->findChild<QLabel *>("deviceRefreshStatus"); QVERIFY(refresh);
            QVERIFY(!refresh->text().contains(QStringLiteral("Dữ liệu có thể đã cũ")));
            qInfo() << "ADMIN_CHILD_STOP_CONFIRMED";
        }
    }
};
QTEST_MAIN(FullUiDesktopAcceptance)
#include "FullUiDesktopAcceptance.moc"
