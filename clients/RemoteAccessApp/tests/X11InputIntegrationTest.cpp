#include <QtTest>
#include <QGuiApplication>
#include <QScreen>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include "Controllers/RemoteInputInjector.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include "Streaming/ScreenCapture.h"

// Run only against a private Xvfb display, never the user's desktop.
class X11InputIntegrationTest : public QObject {
    Q_OBJECT
private slots:
    void capturesDesktopAndInjectsNativeMouseAndKeyboard() {
        QVERIFY2(qEnvironmentVariable("PBL4_ISOLATED_X11") == "1", "Requires a private Xvfb display");
        QCOMPARE(QGuiApplication::platformName(), QStringLiteral("xcb"));
        QWidget window;
        window.move(100, 100);
        window.resize(400, 240);
        window.setAutoFillBackground(true);
        QPalette palette = window.palette();
        palette.setColor(QPalette::Window, Qt::magenta);
        window.setPalette(palette);
        auto *layout = new QVBoxLayout(&window);
        auto *button = new QPushButton("Native click", &window);
        auto *edit = new QLineEdit(&window);
        layout->addWidget(button);
        layout->addWidget(edit);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        const auto frame = ScreenCapture().capture();
        QVERIFY(!frame.isNull());
        const auto geometry = QGuiApplication::primaryScreen()->geometry();
        const auto marker = window.mapToGlobal(QPoint(2, 2)) - geometry.topLeft();
        QCOMPARE(frame.pixelColor(marker.x() * frame.width() / geometry.width(),
                                  marker.y() * frame.height() / geometry.height()), QColor(Qt::magenta));
        RemoteInputInjector injector;
        injector.setFrameSize(frame.size());
        auto click = [&](QWidget *widget) {
            const auto point = widget->mapToGlobal(widget->rect().center()) - geometry.topLeft();
            const auto payload = RemoteInputPacket::button(point.x() * frame.width() / geometry.width(),
                                                           point.y() * frame.height() / geometry.height(), 1);
            QVERIFY(injector.inject(Protocol::MessageType::MOUSE_BUTTON_DOWN, payload));
            QVERIFY(injector.inject(Protocol::MessageType::MOUSE_BUTTON_UP, payload));
        };
        QSignalSpy clicked(button, &QPushButton::clicked);
        click(button);
        QTRY_COMPARE(clicked.count(), 1);
        click(edit);
        QTRY_VERIFY(edit->hasFocus());
        QVERIFY(injector.inject(Protocol::MessageType::KEY_PRESS, RemoteInputPacket::key(Qt::Key_A)));
        QVERIFY(injector.inject(Protocol::MessageType::KEY_RELEASE, RemoteInputPacket::key(Qt::Key_A)));
        QTRY_COMPARE(edit->text(), QStringLiteral("a"));
        QVERIFY(injector.inject(Protocol::MessageType::KEY_PRESS, RemoteInputPacket::key(Qt::Key_Shift)));
        injector.releaseAll();
        QVERIFY(injector.inject(Protocol::MessageType::KEY_PRESS, RemoteInputPacket::key(Qt::Key_C)));
        QVERIFY(injector.inject(Protocol::MessageType::KEY_RELEASE, RemoteInputPacket::key(Qt::Key_C)));
        QTRY_COMPARE(edit->text(), QStringLiteral("ac"));
    }
};
QTEST_MAIN(X11InputIntegrationTest)
#include "X11InputIntegrationTest.moc"
