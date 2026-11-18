#include "LoginSessionMonitor.h"
#include "ApiClient.h"
#include <QEventLoop>

LoginSessionMonitor::LoginSessionMonitor(QObject *parent) : QObject(parent) {
    m_timer.setInterval(5000);
    connect(&m_timer, &QTimer::timeout, this, &LoginSessionMonitor::renew);
}
void LoginSessionMonitor::start() {
    m_loggedOut = false;
    m_lastConfirmed.start();
    m_timer.start();
    renew();
}
void LoginSessionMonitor::stop() { m_timer.stop(); }
void LoginSessionMonitor::renew() {
    if (!m_timer.isActive() || m_inFlight) return;
    if (m_lastConfirmed.elapsed() >= 60000) {
        stop();
        emit sessionLost(QStringLiteral("Mất kết nối máy chủ quá lâu. Vui lòng đăng nhập lại."));
        return;
    }
    auto *reply = ApiClient::instance().post("/api/v1/auth/heartbeat", {});
    if (!reply) return;
    m_inFlight = true;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_inFlight = false;
        const auto result = ApiClient::parseReply(reply);
        reply->deleteLater();
        if (!m_timer.isActive()) return;
        if (result.success) m_lastConfirmed.restart();
        else if (result.httpStatusCode == 401 || result.httpStatusCode == 403) {
            stop();
            emit sessionLost(QStringLiteral("Phiên đăng nhập đã kết thúc. Vui lòng đăng nhập lại."));
        }
    });
}

void LoginSessionMonitor::logout() {
    stop();
    if (m_loggedOut) return;
    m_loggedOut = true;
    auto *reply = ApiClient::instance().post("/api/v1/auth/logout", {});
    if (!reply) return;
    QEventLoop closing;
    QTimer deadline;
    deadline.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &closing, &QEventLoop::quit);
    connect(&deadline, &QTimer::timeout, &closing, &QEventLoop::quit);
    deadline.start(1500);
    if (!reply->isFinished()) closing.exec();
    if (!reply->isFinished()) reply->abort();
    reply->deleteLater();
}
