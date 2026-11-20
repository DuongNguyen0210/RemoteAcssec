#include <QtTest>
#include <memory>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QScopeGuard>
#include "Utils/SystemTelemetrySampler.h"
#include "Network/Http/TelemetryJson.h"
#include "Network/Http/HeartbeatReporter.h"
#include "Network/Http/DeviceService.h"
#include "Network/Http/LoginSessionMonitor.h"
#include "Network/Http/ApiClient.h"
#include "Domain/Store/DeviceStore.h"
#include "Controllers/DevicesController.h"
#include "GUI/Pages/DevicesPage.h"
#include "GUI/Components/DeviceCardWidget.h"
#include "GUI/AppTheme.h"

class TelemetryFlowTest : public QObject {
    Q_OBJECT
    using Counters = SystemTelemetrySampler::Counters;
    QString login(const QString &name, const QString &password) {
        auto &api=ApiClient::instance();
        auto *reply=api.post("/api/v1/auth/login",{{"username",name},{"password",password}});
        QSignalSpy done(reply,&QNetworkReply::finished);
        if (!reply->isFinished()) done.wait(15000);
        auto response=ApiClient::parseReply(reply);
        reply->deleteLater();
        return response.success ? response.data.toObject()["token"].toString() : QString();
    }
private slots:
    void initTestCase() {
#ifdef Q_OS_WIN
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf"));
#endif
        applyAppTheme(*qApp);
    }
    void correctDeltasAndFirstSampleIsUnknown() {
        Counters first;
        first.monotonicMs=1000; first.cpuIdle=100; first.cpuTotal=1000;
        first.memoryTotal=16000; first.memoryAvailable=6000;
        first.networkAvailable=true; first.interfaces.insert(1,qMakePair(quint64(5000),quint64(2000)));
        const auto warm=SystemTelemetrySampler::calculate({},first);
        QVERIFY(!warm.cpuPercent); QVERIFY(!warm.networkRxBytesPerSecond);
        QCOMPARE(*warm.memoryUsedBytes,quint64(10000));
        auto current=first; current.monotonicMs=3000;current.cpuTotal=1400;current.cpuIdle=200;
        current.interfaces[1]=qMakePair(quint64(9096),quint64(3024));
        const auto value=SystemTelemetrySampler::calculate(first,current);
        QCOMPARE(*value.cpuPercent,75.0);
        QCOMPARE(*value.networkRxBytesPerSecond,2048.0);
        QCOMPARE(*value.networkTxBytesPerSecond,512.0);
        const auto previous=current;
        current.monotonicMs=5000;current.cpuTotal=1800;current.cpuIdle=600;
        const auto idle=SystemTelemetrySampler::calculate(previous,current);
        QCOMPARE(*idle.cpuPercent,0.0);
        QVERIFY(!SystemTelemetrySampler::calculate(current,current).cpuPercent);
    }
    void counterResetAndAdapterChangesDoNotCreateSpikes() {
        Counters p; p.monotonicMs=1000;p.cpuTotal=1000;p.cpuIdle=100;
        p.networkAvailable=true;p.interfaces.insert(1,{10000,10000});
        auto c=p;c.monotonicMs=2000;c.cpuTotal=10;c.cpuIdle=5;c.interfaces[1]={10,10};
        auto reset=SystemTelemetrySampler::calculate(p,c);
        QVERIFY(!reset.cpuPercent);QVERIFY(!reset.networkRxBytesPerSecond);
        auto next=c;next.monotonicMs=4000;next.cpuTotal=210;next.cpuIdle=105;next.interfaces[1]={2010,1010};
        auto recovered=SystemTelemetrySampler::calculate(c,next);
        QCOMPARE(*recovered.cpuPercent,50.0);QCOMPARE(*recovered.networkRxBytesPerSecond,1000.0);
        next.interfaces.insert(2,{999999999,999999999});
        QVERIFY(!SystemTelemetrySampler::calculate(c,next).networkRxBytesPerSecond);
        c.memoryTotal=100;c.memoryAvailable=101;
        QVERIFY(!SystemTelemetrySampler::calculate({},c).memoryUsedBytes);
    }
    void zeroTrafficIsDifferentFromUnavailable() {
        Counters p;p.monotonicMs=1;p.networkAvailable=true;p.interfaces.insert(1,{50,60});
        auto c=p;c.monotonicMs=2001;
        auto idle=SystemTelemetrySampler::calculate(p,c);
        QVERIFY(idle.networkRxBytesPerSecond);QCOMPARE(*idle.networkRxBytesPerSecond,0.0);
        c.interfaces.clear();QVERIFY(!SystemTelemetrySampler::calculate(p,c).networkRxBytesPerSecond);
        c=p;c.monotonicMs=0;QVERIFY(!SystemTelemetrySampler::calculate(p,c).networkRxBytesPerSecond);
    }
    void nativeSamplerUsesRealOsCounters() {
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
        SystemTelemetrySampler sampler;
        auto first=sampler.sample();
        QVERIFY(first.memoryTotalBytes && *first.memoryTotalBytes>0);
        QVERIFY(first.memoryUsedBytes && *first.memoryUsedBytes<=*first.memoryTotalBytes);
        QVERIFY(!first.cpuPercent);QVERIFY(!first.networkRxBytesPerSecond);
        QTest::qWait(150);
        auto second=sampler.sample();
        QVERIFY(second.cpuPercent && *second.cpuPercent>=0 && *second.cpuPercent<=100);
        if (second.networkRxBytesPerSecond) QVERIFY(*second.networkRxBytesPerSecond>=0);
        sampler.reset();QVERIFY(!sampler.sample().cpuPercent);
#endif
    }
    void strictJsonAndBackwardCompatibility() {
        TelemetryInfo sample;sample.memoryTotalBytes=16000;sample.memoryUsedBytes=0;
        const auto json=TelemetryJson::encode(sample,1);
        QVERIFY(json["cpuPercent"].isNull());
        auto decoded=TelemetryJson::decode(json,0);
        QVERIFY(decoded);QCOMPARE(*decoded->memoryUsedBytes,quint64(0));
        QVERIFY(!TelemetryJson::decode(QJsonValue(),0));
        auto bad=json;bad["cpuPercent"]=101;QVERIFY(!TelemetryJson::decode(bad,0));
        bad=json;bad["cpuPercent"]="20";QVERIFY(!TelemetryJson::decode(bad,0));
        bad=json;bad["memoryUsedBytes"]=17000;QVERIFY(!TelemetryJson::decode(bad,0));
        bad=json;bad["networkRxBytesPerSecond"]=1;QVERIFY(!TelemetryJson::decode(bad,0));
        QVERIFY(!TelemetryJson::decode(json,-1));
        bad=json;bad["sequence"]=1.5;QVERIFY(!TelemetryJson::decode(bad,0));
    }
    void uiShowsUnitsUnknownAndStaleData() {
        DeviceInfo info;info.sessionId="fixture";info.deviceName=QStringLiteral("Máy CHILD");
        TelemetryInfo t;t.sequence=5;t.cpuPercent=37.5;t.memoryUsedBytes=4ull*1024*1024*1024;
        t.memoryTotalBytes=16ull*1024*1024*1024;t.networkRxBytesPerSecond=2048;t.networkTxBytesPerSecond=0;
        info.telemetry=t;DeviceCardWidget card(info);
        QCOMPARE(card.findChild<QLabel*>("telemetryCpu")->text(),QStringLiteral("37.5 %"));
        QVERIFY(card.findChild<QLabel*>("telemetryMemory")->text().contains("4.0 / 16.0 GiB"));
        QCOMPARE(card.findChild<QLabel*>("telemetryRx")->text(),QStringLiteral("2.0 KiB/s"));
        QCOMPARE(card.findChild<QLabel*>("telemetryTx")->text(),QStringLiteral("0.0 B/s"));
        info.telemetry->ageMs=TelemetryInfo::STALE_AFTER_MS-500;
        DeviceCardWidget aging(info);
        QTRY_COMPARE_WITH_TIMEOUT(aging.findChild<QLabel*>("telemetryState")->text(),QStringLiteral("Mất cập nhật số liệu"),2000);
        QCOMPARE(aging.findChild<QLabel*>("telemetryCpu")->text(),QStringLiteral("—"));
        info.telemetry.reset();DeviceCardWidget oldClient(info);
        QCOMPARE(oldClient.findChild<QLabel*>("telemetryState")->text(),QStringLiteral("Chưa có số liệu"));
        const auto dir=qEnvironmentVariable("TEST_SCREENSHOT_DIR");
        if (!dir.isEmpty()) { QDir().mkpath(dir);card.resize(420,520);card.show();QTest::qWait(30);QVERIFY(card.grab().save(dir+"/telemetry-card.png")); }
    }
    void heartbeatPublishesRealSamplesAndStopsWithConsent() {
        auto &api=ApiClient::instance();const auto old=api.getBaseUrl();
        const auto restore=qScopeGuard([&]{api.setBaseUrl(old);});
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));
        QList<QJsonObject> samples;QStringList paths;
        connect(&server,&QTcpServer::newConnection,this,[&]{
            auto *socket=server.nextPendingConnection();auto buffer=std::make_shared<QByteArray>();
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket,buffer]{
                buffer->append(socket->readAll());int boundary=buffer->indexOf("\r\n\r\n");if(boundary<0)return;
                const auto header=buffer->left(boundary);int length=0;
                for(auto line:header.split('\n')) if(line.toLower().startsWith("content-length:")) length=line.mid(15).trimmed().toInt();
                if(buffer->size()<boundary+4+length)return;
                const auto route=QString::fromLatin1(header.split(' ').value(1));paths.append(route);
                if(route.endsWith("/heartbeat")) samples.append(QJsonDocument::fromJson(buffer->mid(boundary+4,length)).object()["telemetry"].toObject());
                const QByteArray body="{\"success\":true}";
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);
                socket->disconnectFromHost();
            });
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
        api.setBaseUrl("http://127.0.0.1:"+QString::number(server.serverPort()));
        HeartbeatReporter reporter;reporter.start();
        QTRY_COMPARE(samples.size(),1);QVERIFY(samples.first()["cpuPercent"].isNull());
        QTRY_VERIFY_WITH_TIMEOUT(samples.size()>=2,5000);
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
        QVERIFY(samples.last()["cpuPercent"].isDouble());QVERIFY(samples.last()["memoryTotalBytes"].toDouble()>0);
#endif
        const auto sequence=samples.last()["sequence"].toDouble();
        reporter.stop();QTRY_VERIFY(paths.last().endsWith("/unshare"));
        const auto count=samples.size();QTest::qWait(2200);QCOMPARE(samples.size(),count);
        reporter.start();QTRY_VERIFY(samples.size()>count);
        QVERIFY(samples.last()["sequence"].toDouble()>sequence);QVERIFY(samples.last()["cpuPercent"].isNull());
        reporter.stop();QTRY_VERIFY(paths.last().endsWith("/unshare"));
    }
    void liveChildPublisher() {
        if(qgetenv("PBL4_TELEMETRY_CHILD")!="1") QSKIP("Child fixture process only");
        auto &api=ApiClient::instance();api.setBaseUrl(qEnvironmentVariable("REMOTE_API_URL"));
        const auto token=login(qEnvironmentVariable("PBL4_TELEMETRY_CHILD_USER"),qEnvironmentVariable("PBL4_TELEMETRY_PASSWORD"));
        QVERIFY(!token.isEmpty());api.setToken(token);
        LoginSessionMonitor monitor;monitor.start();HeartbeatReporter reporter;reporter.start();
        QTest::qWait(12000);reporter.stop();QTest::qWait(2000);monitor.logout();
    }
    void liveEndToEnd() {
        if(qgetenv("PBL4_TELEMETRY_LIVE")!="1") QSKIP("Set isolated TLS telemetry acceptance environment");
        auto &api=ApiClient::instance();api.setBaseUrl(qEnvironmentVariable("REMOTE_API_URL"));
        const auto password=qEnvironmentVariable("PBL4_TELEMETRY_PASSWORD");
        const auto token=login(qEnvironmentVariable("PBL4_TELEMETRY_ADMIN_USER"),password);
        QVERIFY(!token.isEmpty());api.setToken(token);
        LoginSessionMonitor monitor;monitor.start();
        const auto logout=qScopeGuard([&]{monitor.logout();});
        DeviceStore store;DeviceService service;DevicesController controller(&store,&service);
        controller.refresh();
        QString previousSession;
        for(int round=0;round<2;++round) {
            QProcess child;auto env=QProcessEnvironment::systemEnvironment();env.insert("PBL4_TELEMETRY_CHILD","1");
            child.setProcessEnvironment(env);
            const auto childLog=QDir(qEnvironmentVariable("PBL4_TELEMETRY_RESULT_DIR")).filePath(QString("qt-telemetry-child-%1.txt").arg(round));
            child.start(QCoreApplication::applicationFilePath(),{"liveChildPublisher","-o",childLog+",txt"});
            QVERIFY(child.waitForStarted(5000));
            const auto terminate=qScopeGuard([&]{if(child.state()!=QProcess::NotRunning){child.kill();child.waitForFinished(3000);}});
            const auto ready=[&]{
                return store.getDevices().size()==1 && store.getDevices().first().telemetry
                    && store.getDevices().first().telemetry->cpuPercent
                    && store.getDevices().first().telemetry->memoryTotalBytes
                    && store.getDevices().first().telemetry->networkRxBytesPerSecond;
            };
            QTRY_VERIFY_WITH_TIMEOUT(ready(),15000);
            const auto first=store.getDevices().first();
            QVERIFY(first.sessionId!=previousSession);previousSession=first.sessionId;
            const auto sequence=first.telemetry->sequence;
            QTRY_VERIFY_WITH_TIMEOUT(ready() && store.getDevices().first().telemetry->sequence>sequence,10000);
            auto *page=controller.getView();
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            auto *cpu=page->findChild<QLabel*>("telemetryCpu");QVERIFY(cpu);QVERIFY(cpu->text().contains('%'));
            auto *memory=page->findChild<QLabel*>("telemetryMemory");QVERIFY(memory);QVERIFY(memory->text().contains("GiB"));
            auto *rx=page->findChild<QLabel*>("telemetryRx");QVERIFY(rx);QVERIFY(rx->text().contains("/s"));
            const auto dir=qEnvironmentVariable("TEST_SCREENSHOT_DIR");
            if(!dir.isEmpty()) {QDir().mkpath(dir);page->resize(950,820);page->show();QTest::qWait(50);QVERIFY(page->grab().save(dir+"/telemetry-live-devices.png"));}
            QTRY_COMPARE_WITH_TIMEOUT(child.state(),QProcess::NotRunning,20000);
            QCOMPARE(child.exitStatus(),QProcess::NormalExit);QCOMPARE(child.exitCode(),0);
            QFile output(childLog);QVERIFY(output.open(QIODevice::ReadOnly));
            const auto result=output.readAll();QVERIFY(result.contains("PASS   : TelemetryFlowTest::liveChildPublisher()"));
            QTRY_VERIFY_WITH_TIMEOUT(store.getDevices().isEmpty(),8000);
        }
        service.stopSseStream();
    }
};
QTEST_MAIN(TelemetryFlowTest)
#include "TelemetryFlowTest.moc"
