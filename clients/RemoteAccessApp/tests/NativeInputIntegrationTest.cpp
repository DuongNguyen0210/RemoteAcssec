#include <QtTest>
#include <QGuiApplication>
#include <QScreen>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include "Controllers/RemoteInputInjector.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include "Streaming/ScreenCapture.h"

// Run only against a private Xvfb display or a dedicated Windows test VM.
class NativeInputIntegrationTest : public QObject {
    Q_OBJECT
private slots:
    void capturesDesktopAndInjectsNativeMouseAndKeyboard() {
#ifdef Q_OS_WIN
        QVERIFY2(qEnvironmentVariable("PBL4_ISOLATED_WINDOWS") == "1", "Requires a dedicated Windows test VM");
        QCOMPARE(QGuiApplication::platformName(), QStringLiteral("windows"));
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
        layout->addWidget(button);
        layout->addWidget(edit);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        const auto frame = ScreenCapture().capture();
        QVERIFY(!frame.isNull());
        const auto *screen = QGuiApplication::primaryScreen();
        const auto geometry = screen->geometry();
        const int scale = qEnvironmentVariableIntValue("QT_SCALE_FACTOR");
        QVERIFY(scale == 1 || scale == 2);
        QCOMPARE(screen->devicePixelRatio(), qreal(scale));
        QCOMPARE(frame.size(), geometry.size() * scale);
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
QTEST_MAIN(NativeInputIntegrationTest)
#include "NativeInputIntegrationTest.moc"
