#include <QtTest>
#include <QGuiApplication>
#include <QScreen>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include "Controllers/RemoteInputInjector.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include "Streaming/ScreenCapture.h"
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

// Run only against a private Xvfb display or a dedicated Windows test VM.
class NativeInputIntegrationTest : public QObject {
    Q_OBJECT
private slots:
    void capturesDesktopAndInjectsNativeMouseAndKeyboard() {
#ifdef Q_OS_WIN
        QVERIFY2(qEnvironmentVariable("PBL4_ISOLATED_WINDOWS") == "1", "Requires a dedicated Windows test VM");
        QCOMPARE(QGuiApplication::platformName(), QStringLiteral("windows"));
        DWORD sessionId = 0;
        QVERIFY(ProcessIdToSessionId(GetCurrentProcessId(), &sessionId));
        QVERIFY2(sessionId != 0, "Native input acceptance requires an interactive desktop, not session 0");
        qInfo() << "Windows interactive session" << sessionId;
#else
        QVERIFY2(qEnvironmentVariable("PBL4_ISOLATED_X11") == "1", "Requires a private Xvfb display");
        QCOMPARE(QGuiApplication::platformName(), QStringLiteral("xcb"));
#endif
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
        auto *colorMarker = new QWidget(&window);
        colorMarker->setFixedSize(32, 32);
        colorMarker->setAutoFillBackground(true);
        colorMarker->setPalette(palette);
        layout->addWidget(colorMarker);
        layout->addWidget(button);
        layout->addWidget(edit);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        auto frame = ScreenCapture().capture();
        QVERIFY(!frame.isNull());
        const auto *screen = QGuiApplication::primaryScreen();
        const auto geometry = screen->geometry();
        const int scale = qEnvironmentVariableIntValue("QT_SCALE_FACTOR");
        QVERIFY(scale == 1 || scale == 2);
        QCOMPARE(screen->devicePixelRatio(), qreal(scale));
        QCOMPARE(frame.size(), geometry.size() * scale);
        // Sample a real client-area marker, away from native rounded window corners.
        // Exposure does not guarantee the desktop compositor has presented its paint yet.
        const auto marker = colorMarker->mapToGlobal(colorMarker->rect().center()) - geometry.topLeft();
        QTRY_COMPARE(([&] {
            frame = ScreenCapture().capture();
            return frame.pixelColor(marker.x() * frame.width() / geometry.width(),
                                    marker.y() * frame.height() / geometry.height());
        })(), QColor(Qt::magenta));
        RemoteInputInjector injector;
        // The sender may already be on a different-sized frame when this click arrives.
        injector.setFrameSize(QSize(1, 1));
        auto click = [&](QWidget *widget) {
            const auto point = widget->mapToGlobal(widget->rect().center()) - geometry.topLeft();
            const auto payload = RemoteInputPacket::withFrame(RemoteInputPacket::button(
                point.x() * frame.width() / geometry.width(), point.y() * frame.height() / geometry.height(), 1), frame.size());
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
        const auto point = button->mapToGlobal(button->rect().center()) - geometry.topLeft();
        const auto down = RemoteInputPacket::withFrame(RemoteInputPacket::button(
            point.x() * frame.width() / geometry.width(), point.y() * frame.height() / geometry.height(), 1), frame.size());
        QVERIFY(injector.inject(Protocol::MessageType::MOUSE_BUTTON_DOWN, down));
        QTRY_VERIFY(button->isDown());
        injector.setFrameSize(QSize());
        injector.releaseAll();
        QTRY_VERIFY(!button->isDown());
    }
};
QTEST_MAIN(NativeInputIntegrationTest)
#include "NativeInputIntegrationTest.moc"
