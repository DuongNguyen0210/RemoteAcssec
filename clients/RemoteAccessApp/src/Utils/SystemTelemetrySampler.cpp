#include "SystemTelemetrySampler.h"
#include <QFile>
#include <QDir>
#include <cmath>
#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#endif

SystemTelemetrySampler::SystemTelemetrySampler() { reset(); }
void SystemTelemetrySampler::reset() { m_previous.reset(); m_clock.start(); }
TelemetryInfo SystemTelemetrySampler::sample() {
    const auto current = readCounters();
    auto result = calculate(m_previous, current);
    m_previous = current;
    return result;
}
TelemetryInfo SystemTelemetrySampler::calculate(const std::optional<Counters> &previous, const Counters &c) {
    TelemetryInfo result;
    if (c.memoryTotal && c.memoryAvailable && *c.memoryTotal > 0 && *c.memoryAvailable <= *c.memoryTotal) {
        result.memoryTotalBytes = c.memoryTotal;
        result.memoryUsedBytes = *c.memoryTotal - *c.memoryAvailable;
    }
    if (!previous) return result; // Need two readings for CPU utilization and network rates.
    const auto &p = *previous;
    if (p.cpuTotal && p.cpuIdle && c.cpuTotal && c.cpuIdle
            && *c.cpuTotal > *p.cpuTotal && *c.cpuIdle >= *p.cpuIdle) {
        const quint64 total = *c.cpuTotal - *p.cpuTotal, idle = *c.cpuIdle - *p.cpuIdle;
        if (idle <= total) result.cpuPercent = 100.0 * double(total - idle) / double(total);
    }
    const qint64 elapsed = c.monotonicMs - p.monotonicMs;
    if (elapsed <= 0 || !c.networkAvailable || !p.networkAvailable || c.interfaces.isEmpty()
            || c.interfaces.keys() != p.interfaces.keys()) return result;
    long double rx = 0, tx = 0;
    for (auto it = c.interfaces.cbegin(); it != c.interfaces.cend(); ++it) {
        const auto old = p.interfaces.value(it.key());
        if (it.value().first < old.first || it.value().second < old.second) return result;
        rx += it.value().first - old.first;
        tx += it.value().second - old.second;
    }
    const double rxRate = double(rx * 1000 / elapsed), txRate = double(tx * 1000 / elapsed);
    if (std::isfinite(rxRate) && std::isfinite(txRate) && rxRate <= 1e15 && txRate <= 1e15) {
        result.networkRxBytesPerSecond = rxRate;
        result.networkTxBytesPerSecond = txRate;
    }
    return result;
}
SystemTelemetrySampler::Counters SystemTelemetrySampler::readCounters() const {
    Counters c;
    c.monotonicMs = m_clock.elapsed();
#ifdef Q_OS_WIN
    FILETIME idle{}, kernel{}, user{};
    const auto ticks = [](const FILETIME &v) { return (quint64(v.dwHighDateTime) << 32) | v.dwLowDateTime; };
    if (GetSystemTimes(&idle, &kernel, &user)) {
        c.cpuIdle = ticks(idle);
        c.cpuTotal = ticks(kernel) + ticks(user); // Windows kernel time already includes idle.
    }
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) { c.memoryTotal = memory.ullTotalPhys; c.memoryAvailable = memory.ullAvailPhys; }
    MIB_IF_TABLE2 *table = nullptr;
    if (GetIfTable2(&table) == NO_ERROR) {
        QMap<quint64, QPair<quint64, quint64>> fallback;
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const auto &row = table->Table[i];
            if (row.OperStatus != IfOperStatusUp || row.Type == IF_TYPE_SOFTWARE_LOOPBACK
                    || row.Type == IF_TYPE_TUNNEL || row.InterfaceAndOperStatusFlags.FilterInterface
                    || row.InterfaceAndOperStatusFlags.EndPointInterface) continue;
            const auto bytes = qMakePair(quint64(row.InOctets), quint64(row.OutOctets));
            fallback.insert(row.InterfaceLuid.Value, bytes);
            if (row.InterfaceAndOperStatusFlags.HardwareInterface) c.interfaces.insert(row.InterfaceLuid.Value, bytes);
        }
        // Prefer physical interfaces to avoid counting the same packet again through VPN/virtual layers.
        // A VM with only virtual NICs still reports those interfaces.
        if (c.interfaces.isEmpty()) c.interfaces = fallback;
        c.networkAvailable = true;
        FreeMibTable(table);
    }
#elif defined(Q_OS_LINUX)
    QFile stat("/proc/stat");
    if (stat.open(QIODevice::ReadOnly)) {
        const auto fields = stat.readLine().simplified().split(' ');
        if (fields.size() >= 9 && fields[0] == "cpu") {
            quint64 total = 0;
            for (int i = 1; i <= 8; ++i) total += fields[i].toULongLong(); // Exclude guest times already in user/nice.
            c.cpuTotal = total; c.cpuIdle = fields[4].toULongLong() + fields[5].toULongLong();
        }
    }
    QFile memory("/proc/meminfo");
    if (memory.open(QIODevice::ReadOnly)) {
        for (const auto &rawLine : memory.readAll().split('\n')) {
            const auto line = rawLine.simplified().split(' ');
            if (line.size() < 2) continue;
            if (line[0] == "MemTotal:") c.memoryTotal = line[1].toULongLong() * 1024;
            if (line[0] == "MemAvailable:") c.memoryAvailable = line[1].toULongLong() * 1024;
        }
    }
    QFile net("/proc/net/dev");
    if (net.open(QIODevice::ReadOnly)) {
        QMap<quint64, QPair<quint64, quint64>> fallback;
        for (const auto &line : net.readAll().split('\n')) {
            const auto colon = line.indexOf(':');
            if (colon < 0) continue;
            const auto name = QString::fromLatin1(line.left(colon).trimmed());
            if (name == "lo") continue;
            QFile state("/sys/class/net/" + name + "/operstate"), index("/sys/class/net/" + name + "/ifindex");
            if (!state.open(QIODevice::ReadOnly) || state.readAll().trimmed() != "up" || !index.open(QIODevice::ReadOnly)) continue;
            const auto fields = line.mid(colon + 1).simplified().split(' ');
            if (fields.size() < 9) continue;
            const auto id = index.readAll().trimmed().toULongLong();
            const auto bytes = qMakePair(fields[0].toULongLong(), fields[8].toULongLong());
            fallback.insert(id, bytes);
            if (QDir("/sys/class/net/" + name + "/device").exists()) c.interfaces.insert(id, bytes);
        }
        if (c.interfaces.isEmpty()) c.interfaces = fallback;
        c.networkAvailable = true;
    }
#endif
    return c;
}
