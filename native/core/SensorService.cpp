#include "SensorService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <limits>

#ifdef NEKO_HAS_SENSORS
#include <sensors/sensors.h>
#endif

namespace neko {
namespace {
constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
constexpr int kHistoryLimit = 90;

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

bool readNumber(const QString &path, double &value)
{
    bool ok = false;
    const auto text = readText(path);
    const double parsed = text.toDouble(&ok);
    if (!ok || !std::isfinite(parsed))
        return false;
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

QVariantList boundedHistory(QVariantList history, double value)
{
    history.append(value);
    while (history.size() > kHistoryLimit)
        history.removeFirst();
    return history;
}
} // namespace

SensorService::SensorService(QObject *parent)
    : SensorService(QStringLiteral("/proc"), QStringLiteral("/sys"), parent)
{
}

SensorService::SensorService(QString procRoot, QString sysRoot, QObject *parent)
    : QObject(parent), procRoot_(QDir::cleanPath(std::move(procRoot))), sysRoot_(QDir::cleanPath(std::move(sysRoot)))
{
    summary_ = {{QStringLiteral("cpuUsage"), -1.0}, {QStringLiteral("memoryUsage"), -1.0},
                {QStringLiteral("memoryUsedGiB"), 0.0}, {QStringLiteral("memoryTotalGiB"), 0.0},
                {QStringLiteral("cpuTemperature"), -1.0}, {QStringLiteral("cpuFrequencyGHz"), -1.0},
                {QStringLiteral("cpuPower"), -1.0}, {QStringLiteral("sensorCount"), 0}};
    loadFavorites();
}

QVariantList SensorService::readings() const { return readings_; }
QVariantList SensorService::cpuHistory() const { return cpuHistory_; }
QVariantList SensorService::temperatureHistory() const { return temperatureHistory_; }
QVariantMap SensorService::summary() const { return summary_; }
bool SensorService::paused() const { return paused_; }

void SensorService::setPaused(bool paused)
{
    if (paused_ == paused)
        return;
    paused_ = paused;
    emit pausedChanged();
}

void SensorService::toggleFavorite(const QString &id)
{
    if (favorites_.contains(id)) favorites_.remove(id); else favorites_.insert(id);
    QSettings settings;
    settings.setValue(QStringLiteral("neko/sensorFavorites"), QStringList(favorites_.begin(), favorites_.end()));
    for (auto &entry : readings_) {
        auto reading = entry.toMap();
        if (reading.value(QStringLiteral("id")).toString() == id)
            reading.insert(QStringLiteral("favorite"), favorites_.contains(id));
        entry = reading;
    }
    emit readingsChanged();
}

void SensorService::loadFavorites()
{
    QSettings settings;
    const auto stored = settings.value(QStringLiteral("neko/sensorFavorites")).toStringList();
    favorites_ = QSet<QString>(stored.begin(), stored.end());
}

void SensorService::appendMetric(const QString &id, const QString &name, const QString &group,
                                 double value, const QString &unit, const QString &source,
                                 double minimum, double maximum)
{
    if (!std::isfinite(value))
        return;
    auto it = metrics_.find(id);
    if (it == metrics_.end()) {
        Metric metric;
        metric.name = name;
        metric.group = group;
        metric.unit = unit;
        metric.source = source;
        metric.minimum = minimum;
        metric.maximum = maximum;
        it = metrics_.insert(id, metric);
    }
    auto &metric = it.value();
    metric.current = value;
    metric.minimum = std::min(metric.minimum, value);
    metric.maximum = std::max(metric.maximum, value);
    metric.total += value;
    ++metric.count;

    readings_.append({{QStringLiteral("id"), id}, {QStringLiteral("name"), name},
                      {QStringLiteral("group"), group}, {QStringLiteral("value"), value},
                      {QStringLiteral("unit"), unit}, {QStringLiteral("min"), metric.minimum},
                      {QStringLiteral("max"), metric.maximum},
                      {QStringLiteral("avg"), metric.total / static_cast<double>(metric.count)},
                      {QStringLiteral("favorite"), favorites_.contains(id)},
                      {QStringLiteral("source"), source}, {QStringLiteral("status"), QStringLiteral("available")}});
}

void SensorService::collect(QVariantMap &values, QVariantMap &sources, QVariantMap &groups,
                            QVariantMap &names, QVariantMap &units, QVariantMap &mins, QVariantMap &maxs)
{
    auto add = [&](const QString &id, const QString &name, const QString &group, double value,
                   const QString &unit, const QString &source, double minimum = 0.0, double maximum = 0.0) {
        if (!std::isfinite(value)) return;
        values.insert(id, value); sources.insert(id, source); groups.insert(id, group);
        names.insert(id, name); units.insert(id, unit); mins.insert(id, minimum); maxs.insert(id, maximum);
    };

    // /proc/stat CPU counters are cumulative; usage is derived from two actual samples.
    QFile stat(procRoot_ + QStringLiteral("/stat"));
    if (stat.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&stat);
        while (!stream.atEnd()) {
            const auto line = stream.readLine();
            const auto parts = fields(line);
            if (parts.isEmpty() || !parts.first().startsWith(QStringLiteral("cpu"))) break;
            const QString cpuId = parts.first();
            quint64 total = 0, idle = 0;
            for (qsizetype i = 1; i < parts.size(); ++i) {
                bool ok = false;
                const auto counter = parts.at(i).toULongLong(&ok);
                if (!ok) continue;
                total += counter;
                if (i == 4 || i == 5) idle += counter;
            }
            const auto previous = previousCpuTimes_.constFind(cpuId);
            const quint64 previousTotal = previous == previousCpuTimes_.cend() ? total : previous.value() >> 32U;
            const quint64 previousIdle = previous == previousCpuTimes_.cend() ? idle : previous.value() & 0xffffffffULL;
            previousCpuTimes_.insert(cpuId, (total << 32U) | (idle & 0xffffffffULL));
            const quint64 deltaTotal = total >= previousTotal ? total - previousTotal : 0;
            const quint64 deltaIdle = idle >= previousIdle ? idle - previousIdle : 0;
            if (deltaTotal > 0) {
                const double usage = 100.0 * static_cast<double>(deltaTotal - std::min(deltaTotal, deltaIdle)) / static_cast<double>(deltaTotal);
                const bool aggregate = cpuId == QStringLiteral("cpu");
                const QString id = aggregate ? QStringLiteral("cpu.usage") : QStringLiteral("cpu.core.") + cpuId.mid(3) + QStringLiteral(".usage");
                const QString name = aggregate ? QStringLiteral("CPU utilization") : QStringLiteral("Core %1 utilization").arg(cpuId.mid(3));
                add(id, name, aggregate ? QStringLiteral("CPU") : QStringLiteral("CPU cores"), usage, QStringLiteral("%"), procRoot_ + QStringLiteral("/stat"), 0, 100);
            }
        }
    }

    // Memory and swap counters are exported in KiB by the kernel.
    QMap<QString, quint64> mem;
    QFile meminfo(procRoot_ + QStringLiteral("/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&meminfo);
        while (!stream.atEnd()) {
            const auto parts = fields(stream.readLine());
            if (parts.size() < 2) continue;
            bool ok = false;
            const auto value = parts.at(1).toULongLong(&ok);
            if (ok) mem.insert(parts.first().chopped(1), value);
        }
    }
    if (mem.contains(QStringLiteral("MemTotal")) && mem.contains(QStringLiteral("MemAvailable"))) {
        const double total = static_cast<double>(mem.value(QStringLiteral("MemTotal"))) * 1024.0;
        const double used = std::max(0.0, total - static_cast<double>(mem.value(QStringLiteral("MemAvailable"))) * 1024.0);
        add(QStringLiteral("memory.used"), QStringLiteral("Memory used"), QStringLiteral("Memory"), used / kGiB, QStringLiteral("GiB"), procRoot_ + QStringLiteral("/meminfo"), 0, total / kGiB);
        add(QStringLiteral("memory.usage"), QStringLiteral("Memory utilization"), QStringLiteral("Memory"), total > 0 ? used * 100.0 / total : 0.0, QStringLiteral("%"), procRoot_ + QStringLiteral("/meminfo"), 0, 100);
        add(QStringLiteral("memory.total"), QStringLiteral("Memory total"), QStringLiteral("Memory"), total / kGiB, QStringLiteral("GiB"), procRoot_ + QStringLiteral("/meminfo"));
    }
    if (mem.contains(QStringLiteral("SwapTotal")) && mem.value(QStringLiteral("SwapTotal")) > 0 && mem.contains(QStringLiteral("SwapFree"))) {
        const double total = static_cast<double>(mem.value(QStringLiteral("SwapTotal"))) / (1024.0 * 1024.0);
        const double used = static_cast<double>(mem.value(QStringLiteral("SwapTotal")) - std::min(mem.value(QStringLiteral("SwapTotal")), mem.value(QStringLiteral("SwapFree")))) / (1024.0 * 1024.0);
        add(QStringLiteral("swap.used"), QStringLiteral("Swap used"), QStringLiteral("Memory"), used, QStringLiteral("GiB"), procRoot_ + QStringLiteral("/meminfo"), 0, total);
        add(QStringLiteral("swap.usage"), QStringLiteral("Swap utilization"), QStringLiteral("Memory"), used * 100.0 / total, QStringLiteral("%"), procRoot_ + QStringLiteral("/meminfo"), 0, 100);
    }

    const auto loadavg = readText(procRoot_ + QStringLiteral("/loadavg")).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (!loadavg.isEmpty()) { bool ok = false; const double value = loadavg.first().toDouble(&ok); if (ok) add(QStringLiteral("system.load1"), QStringLiteral("Load average (1 min)"), QStringLiteral("System"), value, QStringLiteral(""), procRoot_ + QStringLiteral("/loadavg")); }

    // Frequency policy and CPU power are reported as physical kernel attributes.
    QDir policies(sysRoot_ + QStringLiteral("/devices/system/cpu/cpufreq"));
    for (const auto &policy : policies.entryList(QStringList{QStringLiteral("policy*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        double khz = 0;
        if (readNumber(policies.filePath(policy + QStringLiteral("/scaling_cur_freq")), khz)) {
            add(QStringLiteral("cpu.frequency.") + policy.mid(6), QStringLiteral("CPU frequency ") + policy.mid(6), QStringLiteral("CPU"), khz / 1000000.0, QStringLiteral("GHz"), policies.filePath(policy + QStringLiteral("/scaling_cur_freq")));
            if (!values.contains(QStringLiteral("cpu.frequency"))) add(QStringLiteral("cpu.frequency"), QStringLiteral("CPU frequency"), QStringLiteral("CPU"), khz / 1000000.0, QStringLiteral("GHz"), policies.filePath(policy + QStringLiteral("/scaling_cur_freq")));
        }
    }
    QDir powercap(sysRoot_ + QStringLiteral("/class/powercap"));
    const auto powerDirs = powercap.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &dir : powerDirs) {
        double microwatts = 0;
        const auto file = powercap.filePath(dir + QStringLiteral("/energy_uj"));
        if (readNumber(file, microwatts)) {
            add(QStringLiteral("cpu.energy.") + dir, displayName(dir) + QStringLiteral(" energy"), QStringLiteral("CPU"), microwatts / 1000000.0, QStringLiteral("J"), file);
            double limit = 0;
            if (readNumber(powercap.filePath(dir + QStringLiteral("/constraint_0_max_power_uw")), limit))
                add(QStringLiteral("cpu.power.limit.") + dir, displayName(dir) + QStringLiteral(" power limit"), QStringLiteral("CPU"), limit / 1000000.0, QStringLiteral("W"), powercap.filePath(dir + QStringLiteral("/constraint_0_max_power_uw")));
        }
    }

    // hwmon values are discovered dynamically. CPU temperature is recognized only for known CPU labels.
    bool cpuTemperatureEmitted = false;
#ifdef NEKO_HAS_SENSORS
    bool usedLibSensors = false;
    if (sensors_init(nullptr) == 0) {
        int chipNumber = 0;
        const sensors_chip_name *chip = nullptr;
        while ((chip = sensors_get_detected_chips(nullptr, &chipNumber)) != nullptr) {
            int featureNumber = 0;
            const sensors_feature *feature = nullptr;
            while ((feature = sensors_get_features(chip, &featureNumber)) != nullptr) {
                int subfeatureNumber = 0;
                const sensors_subfeature *subfeature = nullptr;
                while ((subfeature = sensors_get_all_subfeatures(chip, feature, &subfeatureNumber)) != nullptr) {
                    if (subfeature->type != SENSORS_SUBFEATURE_TEMP_INPUT && subfeature->type != SENSORS_SUBFEATURE_FAN_INPUT &&
                        subfeature->type != SENSORS_SUBFEATURE_IN_INPUT && subfeature->type != SENSORS_SUBFEATURE_POWER_INPUT) continue;
                    double value = 0;
                    if (sensors_get_value(chip, subfeature->number, &value) != 0 || !std::isfinite(value)) continue;
                    char label[256]{};
                    const char *rawLabel = sensors_get_label(chip, feature);
                    qsnprintf(label, sizeof(label), "%s", rawLabel ? rawLabel : feature->name);
                    const QString chipName = QString::fromLocal8Bit(chip->prefix);
                    const QString featureName = QString::fromLocal8Bit(label);
                    const QString kind = subfeature->type == SENSORS_SUBFEATURE_TEMP_INPUT ? QStringLiteral("temp") : subfeature->type == SENSORS_SUBFEATURE_FAN_INPUT ? QStringLiteral("fan") : subfeature->type == SENSORS_SUBFEATURE_IN_INPUT ? QStringLiteral("in") : QStringLiteral("power");
                    const QString id = QStringLiteral("hwmon.") + chipName + QLatin1Char('.') + kind + QLatin1Char('.') + QString::number(feature->number);
                    const QString unit = kind == QStringLiteral("temp") ? QStringLiteral("°C") : kind == QStringLiteral("fan") ? QStringLiteral("RPM") : kind == QStringLiteral("power") ? QStringLiteral("W") : QStringLiteral("V");
                    const QString group = kind == QStringLiteral("temp") ? QStringLiteral("Temperature") : kind == QStringLiteral("fan") ? QStringLiteral("Cooling") : kind == QStringLiteral("power") ? QStringLiteral("Power") : QStringLiteral("Voltage");
                    add(id, chipName + QStringLiteral(" ") + featureName, group, value, unit, QStringLiteral("libsensors:") + chipName);
                    usedLibSensors = true;
                    if (kind == QStringLiteral("temp") && !cpuTemperatureEmitted &&
                        (chipName.contains(QStringLiteral("coretemp"), Qt::CaseInsensitive) || chipName.contains(QStringLiteral("k10temp"), Qt::CaseInsensitive) || chipName.contains(QStringLiteral("x86_pkg_temp"), Qt::CaseInsensitive))) {
                        add(QStringLiteral("cpu.temperature"), QStringLiteral("CPU temperature"), QStringLiteral("CPU"), value, QStringLiteral("°C"), QStringLiteral("libsensors:") + chipName);
                        cpuTemperatureEmitted = true;
                    }
                }
            }
        }
        sensors_cleanup();
    }
    if (!usedLibSensors)
#endif
    {
        QDir hwmon(sysRoot_ + QStringLiteral("/class/hwmon"));
        for (const auto &dir : hwmon.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString base = hwmon.filePath(dir);
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
                const QString label = readText(base + QLatin1Char('/') + type + number + QStringLiteral("_label"));
                const QString suffix = type == QStringLiteral("temp") ? QStringLiteral("temp") : type == QStringLiteral("fan") ? QStringLiteral("fan") : type == QStringLiteral("in") ? QStringLiteral("in") : type;
                const double value = type == QStringLiteral("temp") ? raw / 1000.0 : type == QStringLiteral("power") || type == QStringLiteral("energy") ? raw / 1000000.0 : type == QStringLiteral("in") ? raw / 1000.0 : raw;
                const QString unit = type == QStringLiteral("temp") ? QStringLiteral("°C") : type == QStringLiteral("fan") ? QStringLiteral("RPM") : type == QStringLiteral("in") ? QStringLiteral("V") : type == QStringLiteral("energy") ? QStringLiteral("J") : QStringLiteral("W");
                const QString group = type == QStringLiteral("temp") ? QStringLiteral("Temperature") : type == QStringLiteral("fan") ? QStringLiteral("Cooling") : type == QStringLiteral("in") ? QStringLiteral("Voltage") : QStringLiteral("Power");
                const QString name = chip + QStringLiteral(" ") + (label.isEmpty() ? type + number : label);
                add(QStringLiteral("hwmon.") + chip + QLatin1Char('.') + type + number, name, group, value, unit, file);
                if (type == QStringLiteral("temp") && !cpuTemperatureEmitted &&
                    (chip.contains(QStringLiteral("coretemp"), Qt::CaseInsensitive) || chip.contains(QStringLiteral("k10temp"), Qt::CaseInsensitive) || chip.contains(QStringLiteral("x86_pkg_temp"), Qt::CaseInsensitive))) {
                    add(QStringLiteral("cpu.temperature"), QStringLiteral("CPU temperature"), QStringLiteral("CPU"), value, QStringLiteral("°C"), file);
                    cpuTemperatureEmitted = true;
                }
            }
        }
    }

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
            add(QStringLiteral("battery.") + supply + QLatin1Char('.') + label, supply + QStringLiteral(" ") + displayName(label), QStringLiteral("Battery"), value, unit, file, field == QStringLiteral("capacity") ? 0 : 0, field == QStringLiteral("capacity") ? 100 : 0);
        }
    }

    // Disk sectors and network byte counters are differenced against the prior call.
    QFile diskstats(procRoot_ + QStringLiteral("/diskstats"));
    if (diskstats.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&diskstats);
        while (!stream.atEnd()) {
            const auto parts = fields(stream.readLine());
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
                const quint64 oldValue = old == previousCounters_.cend() ? counter : old.value();
                previousCounters_.insert(key, counter);
                if (old != previousCounters_.cend() && counter >= oldValue)
                    add(rate, device + QLatin1Char(' ') + suffix + QStringLiteral(" throughput"), QStringLiteral("Disk I/O"), static_cast<double>(counter - oldValue) * 512.0 / 1024.0, QStringLiteral("KiB/s"), procRoot_ + QStringLiteral("/diskstats"));
            }
        }
    }
    QDir net(sysRoot_ + QStringLiteral("/class/net"));
    for (const auto &iface : net.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (iface == QStringLiteral("lo")) continue;
        const auto base = net.filePath(iface + QStringLiteral("/statistics"));
        for (const auto &direction : {QStringLiteral("rx"), QStringLiteral("tx")}) {
            const auto file = base + QLatin1Char('/') + direction + QStringLiteral("_bytes");
            double raw = 0;
            if (!readNumber(file, raw) || raw < 0) continue;
            const QString key = QStringLiteral("net.") + iface + QLatin1Char('.') + direction;
            const auto old = previousCounters_.constFind(key);
            const double oldValue = old == previousCounters_.cend() ? raw : static_cast<double>(old.value());
            previousCounters_.insert(key, static_cast<quint64>(raw));
            if (old != previousCounters_.cend() && raw >= oldValue)
                add(key + QStringLiteral(".rate"), iface + QLatin1Char(' ') + direction + QStringLiteral(" throughput"), QStringLiteral("Network"), (raw - oldValue) / 1024.0, QStringLiteral("KiB/s"), file);
        }
    }

    // Optional DRM sysfs counters expose utilization and memory when the driver publishes them.
    QDir drm(sysRoot_ + QStringLiteral("/class/drm"));
    for (const auto &card : drm.entryList(QStringList{QStringLiteral("card[0-9]*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto base = drm.filePath(card + QStringLiteral("/device"));
        double busy = 0;
        if (readNumber(base + QStringLiteral("/gpu_busy_percent"), busy)) add(QStringLiteral("gpu.") + card + QStringLiteral(".usage"), card + QStringLiteral(" utilization"), QStringLiteral("GPU"), busy, QStringLiteral("%"), base + QStringLiteral("/gpu_busy_percent"), 0, 100);
        double total = 0, used = 0;
        if (readNumber(base + QStringLiteral("/mem_info_vram_total"), total) && readNumber(base + QStringLiteral("/mem_info_vram_used"), used) && total > 0) {
            add(QStringLiteral("gpu.") + card + QStringLiteral(".vram_used"), card + QStringLiteral(" VRAM used"), QStringLiteral("GPU memory"), used / kGiB, QStringLiteral("GiB"), base + QStringLiteral("/mem_info_vram_used"), 0, total / kGiB);
            add(QStringLiteral("gpu.") + card + QStringLiteral(".vram_total"), card + QStringLiteral(" VRAM total"), QStringLiteral("GPU memory"), total / kGiB, QStringLiteral("GiB"), base + QStringLiteral("/mem_info_vram_total"));
        }
    }

    QFile thermal(procRoot_ + QStringLiteral("/cpu/thermal_throttle/package_throttle_count"));
    double throttle = 0;
    if (readNumber(thermal.fileName(), throttle)) add(QStringLiteral("cpu.thermal_throttle.package"), QStringLiteral("CPU package thermal throttle count"), QStringLiteral("CPU"), throttle, QStringLiteral("count"), thermal.fileName());
    for (const auto &core : QDir(procRoot_ + QStringLiteral("/cpu")).entryList(QStringList{QStringLiteral("cpu[0-9]*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto file = procRoot_ + QStringLiteral("/cpu/") + core + QStringLiteral("/thermal_throttle/core_throttle_count");
        if (readNumber(file, throttle)) add(QStringLiteral("cpu.thermal_throttle.") + core, core + QStringLiteral(" thermal throttle count"), QStringLiteral("CPU"), throttle, QStringLiteral("count"), file);
    }
}

void SensorService::sample()
{
    QVariantMap values, sources, groups, names, units, mins, maxs;
    collect(values, sources, groups, names, units, mins, maxs);
    const double cpuTemp = values.value(QStringLiteral("cpu.temperature"), -1.0).toDouble();
    if (cpuTemp >= 0.0) {
        emit temperatureObserved(cpuTemp);
        if (!paused_) temperatureHistory_ = boundedHistory(temperatureHistory_, cpuTemp);
    }
    if (paused_) return;

    readings_.clear();
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        appendMetric(it.key(), names.value(it.key()).toString(), groups.value(it.key()).toString(), it.value().toDouble(), units.value(it.key()).toString(), sources.value(it.key()).toString(), mins.value(it.key()).toDouble(), maxs.value(it.key()).toDouble());

    if (values.contains(QStringLiteral("cpu.usage"))) {
        lastCpuUsage_ = values.value(QStringLiteral("cpu.usage")).toDouble();
        cpuHistory_ = boundedHistory(cpuHistory_, lastCpuUsage_);
    }
    if (values.contains(QStringLiteral("memory.used"))) {
        lastMemoryUsedGiB_ = values.value(QStringLiteral("memory.used")).toDouble();
        lastMemoryTotalGiB_ = values.value(QStringLiteral("memory.total")).toDouble();
        lastMemoryUsage_ = values.value(QStringLiteral("memory.usage")).toDouble();
    }
    lastCpuTemperature_ = cpuTemp;
    lastCpuFrequencyGHz_ = values.value(QStringLiteral("cpu.frequency"), -1.0).toDouble();
    lastCpuPower_ = values.value(QStringLiteral("cpu.power"), -1.0).toDouble();
    summary_ = {{QStringLiteral("cpuUsage"), lastCpuUsage_}, {QStringLiteral("memoryUsage"), lastMemoryUsage_},
                {QStringLiteral("memoryUsedGiB"), lastMemoryUsedGiB_}, {QStringLiteral("memoryTotalGiB"), lastMemoryTotalGiB_},
                {QStringLiteral("cpuTemperature"), lastCpuTemperature_}, {QStringLiteral("cpuFrequencyGHz"), lastCpuFrequencyGHz_},
                {QStringLiteral("cpuPower"), lastCpuPower_}, {QStringLiteral("sensorCount"), readings_.size()}};
    emit readingsChanged();
}

void SensorService::resetStatistics()
{
    metrics_.clear();
    cpuHistory_.clear();
    temperatureHistory_.clear();
    if (!paused_) {
        const auto current = readings_;
        readings_.clear();
        for (const auto &entry : current) {
            const auto reading = entry.toMap();
            appendMetric(reading.value(QStringLiteral("id")).toString(), reading.value(QStringLiteral("name")).toString(),
                         reading.value(QStringLiteral("group")).toString(), reading.value(QStringLiteral("value")).toDouble(),
                         reading.value(QStringLiteral("unit")).toString(), reading.value(QStringLiteral("source")).toString(),
                         reading.value(QStringLiteral("min")).toDouble(), reading.value(QStringLiteral("max")).toDouble());
        }
    }
    emit readingsChanged();
}

} // namespace neko
