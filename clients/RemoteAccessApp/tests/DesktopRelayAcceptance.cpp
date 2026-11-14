#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QDebug>
#include "Network/Http/ApiClient.h"
#include "Network/Relay/StaticRelayEndpointProvider.h"
#include "Network/Relay/ChildSessionController.h"
#include "Network/Relay/AdminSessionController.h"
#include "Network/Protocol/RemoteInputPacket.h"
#include "Streaming/ScreenStreamSender.h"
#include "Controllers/RemoteInputInjector.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    const QString role = qEnvironmentVariable("PBL4_DESKTOP_ROLE");
    const QString agent = qEnvironmentVariable("PBL4_DESKTOP_AGENT");
    if (agent.isEmpty() || (role != "child" && role != "admin")) return 2;
    QTimer::singleShot(30000, &app, [&] { qCritical() << "ACCEPTANCE_TIMEOUT"; app.exit(1); });
    if (role == "child") {
        StaticRelayEndpointProvider provider;
        if (qEnvironmentVariable("PBL4_ISOLATED_X11") != "1") return 2;
        ApiClient::instance().setToken("bench-child:" + agent);
        QWidget window;
        window.setGeometry(100, 100, 600, 400);
        window.setAutoFillBackground(true);
        auto color = [&](const QColor &value) {
            QPalette palette = window.palette();
            palette.setColor(QPalette::Window, value); window.setPalette(palette);
        };
        color(Qt::magenta);
        auto *button = new QPushButton("Remote click", &window);
        button->setGeometry(50, 50, 200, 80);
        auto *edit = new QLineEdit(&window);
        edit->setGeometry(50, 160, 300, 60);
        QObject::connect(button, &QPushButton::clicked, &app, [&] { color(Qt::green); });
        QObject::connect(edit, &QLineEdit::textChanged, &app, [&](const QString &text) {
            if (text == "a") color(Qt::blue);
        });
        ChildSessionController child(&provider);
        ScreenStreamSender sender(&child);
        RemoteInputInjector injector;
        QObject::connect(&sender, &ScreenStreamSender::frameCaptured, &app,
                         [&](const QSize &size) { injector.setFrameSize(size); });
        QObject::connect(&child, &ChildSessionController::inputReceived, &app,
                         [&](const Protocol::ProtocolHeader &h, const QByteArray &p) {
            if (!injector.inject(h.type, p)) { qCritical() << "INPUT_REJECTED"; app.exit(1); }
        });
        QObject::connect(&child, &ChildSessionController::relayRegistered, &app,
                         [] { qInfo() << "CHILD_READY"; });
        QObject::connect(&child, &ChildSessionController::sessionFailed, &app,
                         [](const QString &reason) { qWarning() << "CHILD_FAILURE" << reason; });
        bool started = false;
        QObject::connect(&child, &ChildSessionController::sessionStarted, &app,
                         [&](quint64) { started = true; });
        QObject::connect(&child, &ChildSessionController::sessionEnded, &app, [&](const QString &) {
            injector.releaseAll();
            if (started) { qInfo() << "CHILD_DISCONNECTED"; app.exit(0); }
        });
        window.show(); window.activateWindow(); child.start();
        return app.exec();
    }
    ApiClient::instance().setToken("bench-admin");
    AdminSessionController admin(new StaticRelayEndpointProvider, nullptr);
    int stage = 0;
    auto click = [&](int x, int y) {
        const auto payload = RemoteInputPacket::button(x, y, 1);
        return admin.sendInput(Protocol::MessageType::MOUSE_BUTTON_DOWN, payload)
            && admin.sendInput(Protocol::MessageType::MOUSE_BUTTON_UP, payload);
    };
    QObject::connect(&admin, &AdminSessionController::frameReady, &app,
                     [&](quint64, const QImage &image) {
        if (image.size() != QSize(1280, 800)) { qCritical() << "UNEXPECTED_FRAME_SIZE" << image.size(); app.exit(1); return; }
        const auto pixel = image.pixelColor(102, 102);
        if (stage == 0 && pixel.red() > 180 && pixel.blue() > 180 && pixel.green() < 60) {
            qInfo() << "SCREEN_RECEIVED"; stage = 1;
            if (!click(250, 190)) app.exit(1);
        } else if (stage == 1 && pixel.green() > 180 && pixel.red() < 60) {
            qInfo() << "REMOTE_CLICK_CONFIRMED"; stage = 2;
            if (!click(250, 290)
                || !admin.sendInput(Protocol::MessageType::KEY_PRESS, RemoteInputPacket::key(Qt::Key_A))
                || !admin.sendInput(Protocol::MessageType::KEY_RELEASE, RemoteInputPacket::key(Qt::Key_A))) app.exit(1);
        } else if (stage == 2 && pixel.blue() > 180 && pixel.red() < 60 && pixel.green() < 60) {
            qInfo() << "REMOTE_KEY_CONFIRMED"; stage = 3; admin.disconnectSession();
        }
    });
    QObject::connect(&admin, &AdminSessionController::sessionEnded, &app,
                     [&](quint64, const QString &, const QString &) { app.exit(stage == 3 ? 0 : 1); });
    QObject::connect(&admin, &AdminSessionController::sessionFailed, &app,
                     [&](const QString &reason) { qCritical() << reason; app.exit(1); });
    QTimer::singleShot(0, &app, [&] { admin.requestSession(agent); });
    return app.exec();
}
