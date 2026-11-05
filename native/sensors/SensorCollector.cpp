#include "SensorCollector.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace neko {
namespace {
constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}
bool readNumber(const QString &path, double &value)
{
    bool ok = false;
    const double parsed = readText(path).toDouble(&ok);
    if (!ok || !std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}
QStringList fields(const QString &line)
{
    return line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}
QString displayName(QString name)
{
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    return name;
}
struct DeviceIdentity { QString id; QString name; };
DeviceIdentity hwmonDevice(const QString &chip, const QString &canonical)
{
    const QString key = chip.trimmed().toLower();
    static const QSet<QString> cpuChips{QStringLiteral("coretemp"), QStringLiteral("k10temp"),
        QStringLiteral("zenpower"), QStringLiteral("cpu_thermal"), QStringLiteral("x86_pkg_temp")};
    static const QSet<QString> gpuChips{QStringLiteral("amdgpu"), QStringLiteral("i915"),
        QStringLiteral("xe"), QStringLiteral("nouveau"), QStringLiteral("radeon"), QStringLiteral("nvidia")};
    if (cpuChips.contains(key)) return {QStringLiteral("cpu"), QStringLiteral("CPU")};
    if (gpuChips.contains(key)) return {QStringLiteral("gpu"), QStringLiteral("GPU")};
    if (key.startsWith(QStringLiteral("nvme"))) return {QStringLiteral("nvme"), QStringLiteral("NVMe")};
    static const QStringList motherboardHints{QStringLiteral("nct"), QStringLiteral("it87"),
        QStringLiteral("asus"), QStringLiteral("thinkpad"), QStringLiteral("dell"),
        QStringLiteral("acpi"), QStringLiteral("msi"), QStringLiteral("gigabyte"),
        QStringLiteral("asrock"), QStringLiteral("superio"), QStringLiteral("applesmc")};
    for (const auto &hint : motherboardHints)
        if (key.contains(hint)) return {QStringLiteral("motherboard"), QStringLiteral("Motherboard")};
    return {QStringLiteral("hwmon:") + canonical, chip.isEmpty() ? QStringLiteral("Unknown sensor") : chip};
}
}
SensorCollector::SensorCollector(QString procRoot, QString sysRoot)
    : procRoot_(QDir::cleanPath(std::move(procRoot))), sysRoot_(QDir::cleanPath(std::move(sysRoot)))
{
}

SensorSnapshot SensorCollector::collect(Clock::time_point now)
{
    SensorSnapshot snapshot;
    snapshot.collectedAt = QDateTime::currentDateTimeUtc();
    auto &values = snapshot.values;
    auto &sources = snapshot.sources;
    auto &groups = snapshot.groups;
    auto &names = snapshot.names;
    auto &units = snapshot.units;
    auto &deviceIds = snapshot.deviceIds;
    auto &deviceNames = snapshot.deviceNames;
    QMap<QString, CpuTimes> nextCpuTimes;
    QMap<QString, Counter> nextCounters;
    auto addFor = [&](const QString &id, const QString &name, const QString &group, double value,
                      const QString &unit, const QString &source,
                      const QString &deviceId, const QString &deviceName) {
        if (!std::isfinite(value)) return;
        values.insert(id, value); sources.insert(id, source); groups.insert(id, group);
        names.insert(id, name); units.insert(id, unit);
        deviceIds.insert(id, deviceId); deviceNames.insert(id, deviceName);
    };
    auto add = [&](const QString &id, const QString &name, const QString &group, double value,
                   const QString &unit, const QString &source) {
        addFor(id, name, group, value, unit, source, QStringLiteral("system"), QStringLiteral("System"));
    };

    // /proc/stat CPU counters are cumulative; usage is derived from two actual samples.
    QFile stat(procRoot_ + QStringLiteral("/stat"));
    if (stat.open(QIODevice::ReadOnly | QIODevice::Text)) {
        for (const auto &line : QString::fromUtf8(stat.readAll()).split(QLatin1Char('\n'))) {
            const auto parts = fields(line);
            static const QRegularExpression cpuName(QStringLiteral("^cpu[0-9]*$"));
            if (parts.isEmpty() || !cpuName.match(parts.first()).hasMatch()) continue;
            if (parts.size() < 5) continue;
            const QString cpuId = parts.first();
            quint64 total = 0, idle = 0;
            bool valid = true;
            // guest/guest_nice (fields 9/10) are included in user/nice already.
            for (qsizetype i = 1; i < std::min<qsizetype>(parts.size(), 9); ++i) {
                bool ok = false;
                const auto counter = parts.at(i).toULongLong(&ok);
                if (!ok || counter > std::numeric_limits<quint64>::max() - total) { valid = false; break; }
                total += counter;
                if (i == 4 || i == 5) idle += counter;
            }
            if (!valid) continue;
            nextCpuTimes.insert(cpuId, {total, idle});
            const auto previous = previousCpuTimes_.constFind(cpuId);
            if (previous == previousCpuTimes_.cend() || total < previous->total || idle < previous->idle) continue;
            const quint64 deltaTotal = total - previous->total;
            const quint64 deltaIdle = idle - previous->idle;
            if (deltaTotal > 0) {
                const double usage = 100.0 * static_cast<double>(deltaTotal - std::min(deltaTotal, deltaIdle)) / static_cast<double>(deltaTotal);
                const bool aggregate = cpuId == QStringLiteral("cpu");
                const QString id = aggregate ? QStringLiteral("cpu.usage") : QStringLiteral("cpu.core.") + cpuId.mid(3) + QStringLiteral(".usage");
                const QString name = aggregate ? QStringLiteral("CPU utilization") : QStringLiteral("Core %1 utilization").arg(cpuId.mid(3));
                addFor(id, name, aggregate ? QStringLiteral("CPU") : QStringLiteral("CPU cores"), usage, QStringLiteral("%"), procRoot_ + QStringLiteral("/stat"), QStringLiteral("cpu"), QStringLiteral("CPU"));
            }
        }
    }

    // Memory and swap counters are exported in KiB by the kernel.
    QMap<QString, quint64> mem;
    QFile meminfo(procRoot_ + QStringLiteral("/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        for (const auto &line : QString::fromUtf8(meminfo.readAll()).split(QLatin1Char('\n'))) {
            const auto parts = fields(line);
            if (parts.size() < 2) continue;
            bool ok = false;
            const auto value = parts.at(1).toULongLong(&ok);
            if (ok) mem.insert(parts.first().chopped(1), value);
        }
    }
    if (mem.value(QStringLiteral("MemTotal")) > 0 && mem.contains(QStringLiteral("MemAvailable"))) {
        const double total = static_cast<double>(mem.value(QStringLiteral("MemTotal"))) * 1024.0;
        const double used = std::max(0.0, total - static_cast<double>(mem.value(QStringLiteral("MemAvailable"))) * 1024.0);
        addFor(QStringLiteral("memory.used"), QStringLiteral("Memory used"), QStringLiteral("Memory"), used / kGiB, QStringLiteral("GiB"), procRoot_ + QStringLiteral("/meminfo"), QStringLiteral("memory"), QStringLiteral("Memory"));
        addFor(QStringLiteral("memory.usage"), QStringLiteral("Memory utilization"), QStringLiteral("Memory"), total > 0 ? used * 100.0 / total : 0.0, QStringLiteral("%"), procRoot_ + QStringLiteral("/meminfo"), QStringLiteral("memory"), QStringLiteral("Memory"));
        addFor(QStringLiteral("memory.total"), QStringLiteral("Memory total"), QStringLiteral("Memory"), total / kGiB, QStringLiteral("GiB"), procRoot_ + QStringLiteral("/meminfo"), QStringLiteral("memory"), QStringLiteral("Memory"));
    }
    if (mem.contains(QStringLiteral("SwapTotal")) && mem.value(QStringLiteral("SwapTotal")) > 0 && mem.contains(QStringLiteral("SwapFree"))) {
        const double total = static_cast<double>(mem.value(QStringLiteral("SwapTotal"))) / (1024.0 * 1024.0);
        const double used = static_cast<double>(mem.value(QStringLiteral("SwapTotal")) - std::min(mem.value(QStringLiteral("SwapTotal")), mem.value(QStringLiteral("SwapFree")))) / (1024.0 * 1024.0);
        addFor(QStringLiteral("swap.used"), QStringLiteral("Swap used"), QStringLiteral("Memory"), used, QStringLiteral("GiB"), procRoot_ + QStringLiteral("/meminfo"), QStringLiteral("memory"), QStringLiteral("Memory"));
        addFor(QStringLiteral("swap.usage"), QStringLiteral("Swap utilization"), QStringLiteral("Memory"), used * 100.0 / total, QStringLiteral("%"), procRoot_ + QStringLiteral("/meminfo"), QStringLiteral("memory"), QStringLiteral("Memory"));
    }

    const auto loadavg = readText(procRoot_ + QStringLiteral("/loadavg")).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (!loadavg.isEmpty()) { bool ok = false; const double value = loadavg.first().toDouble(&ok); if (ok) add(QStringLiteral("system.load1"), QStringLiteral("Load average (1 min)"), QStringLiteral("System"), value, QStringLiteral(""), procRoot_ + QStringLiteral("/loadavg")); }

    // Per-CPU frequency is the unit s-tui and HWiNFO present, with policy
    // granularity retained as a fallback for kernels that expose only policies.
    QDir cpuRoot(sysRoot_ + QStringLiteral("/devices/system/cpu"));
    auto cpuDirs = cpuRoot.entryList(QStringList{QStringLiteral("cpu[0-9]*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(cpuDirs.begin(), cpuDirs.end(), [](const QString &a, const QString &b) {
        return a.mid(3).toInt() < b.mid(3).toInt();
    });
    double freqSum = 0.0, freqMax = 0.0, freqMin = 0.0;
    int freqCount = 0;
    QStringList freqSources, maxSources, minSources;
    for (const auto &cpu : cpuDirs) {
        const auto base = cpuRoot.filePath(cpu + QStringLiteral("/cpufreq"));
        const auto currentPath = base + QStringLiteral("/scaling_cur_freq");
        double khz = 0;
        if (readNumber(currentPath, khz)) {
            const QString core = cpu.mid(3);
            addFor(QStringLiteral("cpu.frequency.core.") + core,
                   QStringLiteral("Core ") + core + QStringLiteral(" frequency"),
                   QStringLiteral("Frequency"), khz / 1000.0, QStringLiteral("MHz"),
                   currentPath, QStringLiteral("cpu"), QStringLiteral("CPU"));
            freqSum += khz; ++freqCount; freqSources.append(currentPath);
        }
        double maxKhz = 0, minKhz = 0;
        const auto maxPath = base + QStringLiteral("/cpuinfo_max_freq");
        const auto minPath = base + QStringLiteral("/cpuinfo_min_freq");
        if (!readNumber(maxPath, maxKhz)) readNumber(base + QStringLiteral("/scaling_max_freq"), maxKhz);
        if (!readNumber(minPath, minKhz)) readNumber(base + QStringLiteral("/scaling_min_freq"), minKhz);
        if (maxKhz > 0 && maxKhz > freqMax) { freqMax = maxKhz; maxSources = {maxPath}; }
        if (minKhz > 0 && (freqMin == 0.0 || minKhz < freqMin)) { freqMin = minKhz; minSources = {minPath}; }
    }
    if (freqCount > 0) {
        addFor(QStringLiteral("cpu.frequency"), QStringLiteral("CPU frequency"),
               QStringLiteral("Frequency"), freqSum / freqCount / 1000.0, QStringLiteral("MHz"),
               freqSources.join(QStringLiteral("; ")), QStringLiteral("cpu"), QStringLiteral("CPU"));
    } else {
        QDir policies(sysRoot_ + QStringLiteral("/devices/system/cpu/cpufreq"));
        for (const auto &policy : policies.entryList(QStringList{QStringLiteral("policy*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
            double khz = 0;
            const auto path = policies.filePath(policy + QStringLiteral("/scaling_cur_freq"));
            if (!readNumber(path, khz)) continue;
            addFor(QStringLiteral("cpu.frequency.") + policy.mid(6),
                   QStringLiteral("CPU frequency ") + policy.mid(6), QStringLiteral("Frequency"),
                   khz / 1000.0, QStringLiteral("MHz"), path, QStringLiteral("cpu"), QStringLiteral("CPU"));
            freqSum += khz; ++freqCount; freqSources.append(path);
        }
        if (freqCount > 0)
            addFor(QStringLiteral("cpu.frequency"), QStringLiteral("CPU frequency"),
                   QStringLiteral("Frequency"), freqSum / freqCount / 1000.0, QStringLiteral("MHz"),
                   freqSources.join(QStringLiteral("; ")), QStringLiteral("cpu"), QStringLiteral("CPU"));
    }
    if (freqMax > 0)
        addFor(QStringLiteral("cpu.frequency.max"), QStringLiteral("CPU maximum frequency"),
               QStringLiteral("Frequency"), freqMax / 1000.0, QStringLiteral("MHz"),
               maxSources.join(QStringLiteral("; ")), QStringLiteral("cpu"), QStringLiteral("CPU"));
    if (freqMin > 0)
        addFor(QStringLiteral("cpu.frequency.min"), QStringLiteral("CPU minimum frequency"),
               QStringLiteral("Frequency"), freqMin / 1000.0, QStringLiteral("MHz"),
               minSources.join(QStringLiteral("; ")), QStringLiteral("cpu"), QStringLiteral("CPU"));
    QDir powercap(sysRoot_ + QStringLiteral("/class/powercap"));
    double packagePower = 0.0;
    qsizetype packageCount = 0, sampledPackages = 0;
    QStringList packageSources;
    for (const auto &dir : powercap.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto base = powercap.filePath(dir);
        const auto file = base + QStringLiteral("/energy_uj");
        const bool isPackage = readText(base + QStringLiteral("/name")).startsWith(QStringLiteral("package-"));
        if (isPackage) ++packageCount;
        bool ok = false;
        const auto energy = readText(file).toULongLong(&ok);
        if (!ok) continue;
        addFor(QStringLiteral("cpu.energy.") + dir, displayName(dir) + QStringLiteral(" energy"), QStringLiteral("CPU"), static_cast<double>(energy) / 1000000.0, QStringLiteral("J"), file, QStringLiteral("cpu"), QStringLiteral("CPU"));
        const auto key = QStringLiteral("energy.") + dir;
        nextCounters.insert(key, {energy, now});
        const auto previous = previousCounters_.constFind(key);
        if (previous != previousCounters_.cend()) {
            const double seconds = std::chrono::duration<double>(now - previous->at).count();
            quint64 delta = 0;
            bool valid = energy >= previous->value;
            if (valid) delta = energy - previous->value;
            else {
                const auto range = readText(base + QStringLiteral("/max_energy_range_uj")).toULongLong(&ok);
                if (ok && range > previous->value && range > energy) {
                    delta = range - previous->value + energy;
                    valid = true;
                }
            }
            if (valid && seconds > 0.0) {
                const double watts = static_cast<double>(delta) / 1000000.0 / seconds;
                addFor(QStringLiteral("cpu.power.") + dir, displayName(dir) + QStringLiteral(" power"), QStringLiteral("CPU"), watts, QStringLiteral("W"), file, QStringLiteral("cpu"), QStringLiteral("CPU"));
                if (isPackage) { packagePower += watts; ++sampledPackages; packageSources.append(file); }
            }
        }
        double limit = 0;
        const auto limitFile = base + QStringLiteral("/constraint_0_max_power_uw");
        if (readNumber(limitFile, limit))
            addFor(QStringLiteral("cpu.power.limit.") + dir, displayName(dir) + QStringLiteral(" power limit"), QStringLiteral("CPU"), limit / 1000000.0, QStringLiteral("W"), limitFile, QStringLiteral("cpu"), QStringLiteral("CPU"));
    }
    if (packageCount > 0 && packageCount == sampledPackages)
        addFor(QStringLiteral("cpu.power"), QStringLiteral("CPU package power"), QStringLiteral("CPU"), packagePower, QStringLiteral("W"), packageSources.join(QStringLiteral("; ")), QStringLiteral("cpu"), QStringLiteral("CPU"));

    // hwmon values are discovered dynamically. CPU temperature is recognized only for known CPU labels.
    double cpuTemperature = -std::numeric_limits<double>::infinity();
    QString cpuTemperatureSource;
    {
        QDir hwmon(sysRoot_ + QStringLiteral("/class/hwmon"));
        for (const auto &dir : hwmon.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString base = hwmon.filePath(dir);
            const auto canonical = QFileInfo(base).canonicalFilePath();
            const QString identity = canonical.isEmpty() ? QDir::cleanPath(base) : canonical;
            QString chip = readText(base + QStringLiteral("/name"));
            if (chip.isEmpty()) chip = dir;
            const auto entries = QDir(base).entryList(QDir::Files, QDir::Name);
            for (const auto &entry : entries) {
                static const QRegularExpression channel(QStringLiteral("^(temp|fan|in|power|energy)([0-9]+)_input$"));
                const auto match = channel.match(entry);
                if (!match.hasMatch()) continue;
                double raw = 0;
                const auto file = base + QLatin1Char('/') + entry;
                if (!readNumber(file, raw)) continue;
                const QString type = match.captured(1);
                const QString number = match.captured(2);
                const auto prefix = base + QLatin1Char('/') + type + number;
                double flag = 0.0;
                if (readNumber(prefix + QStringLiteral("_fault"), flag) && flag != 0.0) continue;
                if (readNumber(prefix + QStringLiteral("_enable"), flag) && flag == 0.0) continue;
                const QString label = readText(prefix + QStringLiteral("_label"));
                const double value = type == QStringLiteral("temp") ? raw / 1000.0 : type == QStringLiteral("power") || type == QStringLiteral("energy") ? raw / 1000000.0 : type == QStringLiteral("in") ? raw / 1000.0 : raw;
                const QString unit = type == QStringLiteral("temp") ? QStringLiteral("°C") : type == QStringLiteral("fan") ? QStringLiteral("RPM") : type == QStringLiteral("in") ? QStringLiteral("V") : type == QStringLiteral("energy") ? QStringLiteral("J") : QStringLiteral("W");
                const QString group = type == QStringLiteral("temp") ? QStringLiteral("Temperature") : type == QStringLiteral("fan") ? QStringLiteral("Cooling") : type == QStringLiteral("in") ? QStringLiteral("Voltage") : QStringLiteral("Power");
                const QString name = chip + QStringLiteral(" ") + (label.isEmpty() ? type + number : label);
                const auto device = hwmonDevice(chip, identity);
                addFor(QStringLiteral("hwmon.") + identity + QLatin1Char('.') + type + number, name, group, value, unit, file, device.id, device.name);
                if (type == QStringLiteral("temp") && value > cpuTemperature &&
                    (chip.contains(QStringLiteral("coretemp"), Qt::CaseInsensitive) || chip.contains(QStringLiteral("k10temp"), Qt::CaseInsensitive) || chip.contains(QStringLiteral("zenpower"), Qt::CaseInsensitive) || chip.contains(QStringLiteral("cpu_thermal"), Qt::CaseInsensitive) || chip.contains(QStringLiteral("x86_pkg_temp"), Qt::CaseInsensitive))) {
                    cpuTemperature = value;
                    cpuTemperatureSource = file;
                }
            }
        }
    }

    if (std::isfinite(cpuTemperature))
        addFor(QStringLiteral("cpu.temperature"), QStringLiteral("CPU temperature (hottest)"), QStringLiteral("CPU"), cpuTemperature, QStringLiteral("°C"), cpuTemperatureSource, QStringLiteral("cpu"), QStringLiteral("CPU"));

    // Battery attributes use Linux power_supply values and retain each battery's identity.
    QDir supplies(sysRoot_ + QStringLiteral("/class/power_supply"));
    for (const auto &supply : supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto base = supplies.filePath(supply);
        const auto type = readText(base + QStringLiteral("/type"));
        if (type != QStringLiteral("Battery")) continue;
        for (const auto &field : {QStringLiteral("capacity"), QStringLiteral("voltage_now"), QStringLiteral("current_now"), QStringLiteral("power_now"), QStringLiteral("energy_now")}) {
            double raw = 0;
            const auto file = base + QLatin1Char('/') + field;
            if (!readNumber(file, raw)) continue;
            QString unit = field == QStringLiteral("capacity") ? QStringLiteral("%") : field == QStringLiteral("voltage_now") ? QStringLiteral("V") : field == QStringLiteral("current_now") ? QStringLiteral("A") : field == QStringLiteral("energy_now") ? QStringLiteral("Wh") : QStringLiteral("W");
            double value = field == QStringLiteral("voltage_now") ? raw / 1000000.0 : field == QStringLiteral("current_now") ? raw / 1000000.0 : field == QStringLiteral("power_now") ? raw / 1000000.0 : field == QStringLiteral("energy_now") ? raw / 1000000.0 : raw;
            const QString label = field == QStringLiteral("capacity") ? QStringLiteral("charge") : field;
            addFor(QStringLiteral("battery.") + supply + QLatin1Char('.') + label, supply + QStringLiteral(" ") + displayName(label), QStringLiteral("Battery"), value, unit, file, QStringLiteral("battery"), QStringLiteral("Battery"));
        }
    }

    // Disk sectors and network byte counters are differenced against the prior call.
    QFile diskstats(procRoot_ + QStringLiteral("/diskstats"));
    if (diskstats.open(QIODevice::ReadOnly | QIODevice::Text)) {
        for (const auto &line : QString::fromUtf8(diskstats.readAll()).split(QLatin1Char('\n'))) {
            const auto parts = fields(line);
            if (parts.size() < 14) continue;
            const auto device = parts.at(2);
            if (device.startsWith(QStringLiteral("loop")) || device.startsWith(QStringLiteral("ram"))) continue;
            bool readOk = false, writeOk = false;
            const auto readSectors = parts.at(5).toULongLong(&readOk);
            const auto writeSectors = parts.at(9).toULongLong(&writeOk);
            if (!readOk || !writeOk) continue;
            for (const auto &[suffix, counter] : {std::pair<QString, quint64>{QStringLiteral("read"), readSectors}, {QStringLiteral("write"), writeSectors}}) {
                const QString key = QStringLiteral("disk.") + device + QLatin1Char('.') + suffix;
                const QString rate = key + QStringLiteral(".rate");
                const auto old = previousCounters_.constFind(key);
                nextCounters.insert(key, {counter, now});
                if (old == previousCounters_.cend() || counter < old->value) continue;
                const double seconds = std::chrono::duration<double>(now - old->at).count();
                if (seconds > 0.0)
                    addFor(rate, device + QLatin1Char(' ') + suffix + QStringLiteral(" throughput"), QStringLiteral("Disk I/O"), static_cast<double>(counter - old->value) * 512.0 / 1024.0 / seconds, QStringLiteral("KiB/s"), procRoot_ + QStringLiteral("/diskstats"), QStringLiteral("storage"), QStringLiteral("Storage"));
            }
        }
    }
    QDir net(sysRoot_ + QStringLiteral("/class/net"));
    for (const auto &iface : net.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (iface == QStringLiteral("lo")) continue;
        const auto base = net.filePath(iface + QStringLiteral("/statistics"));
        for (const auto &direction : {QStringLiteral("rx"), QStringLiteral("tx")}) {
            const auto file = base + QLatin1Char('/') + direction + QStringLiteral("_bytes");
            bool ok = false;
            const auto counter = readText(file).toULongLong(&ok);
            if (!ok) continue;
            const QString key = QStringLiteral("net.") + iface + QLatin1Char('.') + direction;
            const auto old = previousCounters_.constFind(key);
            nextCounters.insert(key, {counter, now});
            if (old == previousCounters_.cend() || counter < old->value) continue;
            const double seconds = std::chrono::duration<double>(now - old->at).count();
            if (seconds > 0.0)
                addFor(key + QStringLiteral(".rate"), iface + QLatin1Char(' ') + direction + QStringLiteral(" throughput"), QStringLiteral("Network"), static_cast<double>(counter - old->value) / 1024.0 / seconds, QStringLiteral("KiB/s"), file, QStringLiteral("network"), QStringLiteral("Network"));
        }
    }

    // Optional DRM sysfs counters expose utilization and memory when the driver publishes them.
    QDir drm(sysRoot_ + QStringLiteral("/class/drm"));
    for (const auto &card : drm.entryList(QStringList{QStringLiteral("card[0-9]*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto base = drm.filePath(card + QStringLiteral("/device"));
        double busy = 0;
        if (readNumber(base + QStringLiteral("/gpu_busy_percent"), busy)) addFor(QStringLiteral("gpu.") + card + QStringLiteral(".usage"), card + QStringLiteral(" utilization"), QStringLiteral("GPU"), busy, QStringLiteral("%"), base + QStringLiteral("/gpu_busy_percent"), QStringLiteral("gpu"), QStringLiteral("GPU"));
        double total = 0, used = 0;
        if (readNumber(base + QStringLiteral("/mem_info_vram_total"), total) && readNumber(base + QStringLiteral("/mem_info_vram_used"), used) && total > 0) {
            addFor(QStringLiteral("gpu.") + card + QStringLiteral(".vram_used"), card + QStringLiteral(" VRAM used"), QStringLiteral("GPU memory"), used / kGiB, QStringLiteral("GiB"), base + QStringLiteral("/mem_info_vram_used"), QStringLiteral("gpu"), QStringLiteral("GPU"));
            addFor(QStringLiteral("gpu.") + card + QStringLiteral(".vram_total"), card + QStringLiteral(" VRAM total"), QStringLiteral("GPU memory"), total / kGiB, QStringLiteral("GiB"), base + QStringLiteral("/mem_info_vram_total"), QStringLiteral("gpu"), QStringLiteral("GPU"));
        }
    }

    QFile thermal(sysRoot_ + QStringLiteral("/devices/system/cpu/thermal_throttle/package_throttle_count"));
    double throttle = 0;
    if (readNumber(thermal.fileName(), throttle)) addFor(QStringLiteral("cpu.thermal_throttle.package"), QStringLiteral("CPU package thermal throttle count"), QStringLiteral("CPU"), throttle, QStringLiteral("count"), thermal.fileName(), QStringLiteral("cpu"), QStringLiteral("CPU"));
    for (const auto &core : QDir(sysRoot_ + QStringLiteral("/devices/system/cpu")).entryList(QStringList{QStringLiteral("cpu[0-9]*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto file = sysRoot_ + QStringLiteral("/devices/system/cpu/") + core + QStringLiteral("/thermal_throttle/core_throttle_count");
        if (readNumber(file, throttle)) addFor(QStringLiteral("cpu.thermal_throttle.") + core, core + QStringLiteral(" thermal throttle count"), QStringLiteral("CPU"), throttle, QStringLiteral("count"), file, QStringLiteral("cpu"), QStringLiteral("CPU"));
    }
    previousCpuTimes_ = std::move(nextCpuTimes);
    previousCounters_ = std::move(nextCounters);
    return snapshot;
}

} // namespace neko
