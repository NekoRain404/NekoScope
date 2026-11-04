#include "InventoryContext.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTextStream>
#include <QTime>
#include <QUrl>

#include <algorithm>
#include <limits>


namespace neko::inventory {

void Collector::appendCpu(InventorySnapshot &snapshot)
{
    const ReadResult cpuInfoResult = readText(pathFor(QStringLiteral("/proc/cpuinfo")));
    const QString cpuInfo = cpuInfoResult.value;
    const QStringList lines = cpuInfo.split(QLatin1Char('\n'));
    QMap<QString, QString> values;
    for (const QString &line : lines) {
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;
        const QString key = line.left(colon).trimmed();
        if (!values.contains(key))
            values.insert(key, line.mid(colon + 1).trimmed());
    }

    const QString cpuRoot = pathFor(QStringLiteral("/sys/devices/system/cpu"));
    const QStringList cpuDirs = QDir(cpuRoot).entryList({QStringLiteral("cpu[0-9]*")}, QDir::Dirs,
                                                        QDir::Name);
    const QString model = values.value(QStringLiteral("model name"),
                                       values.value(QStringLiteral("Hardware"),
                                                    values.value(QStringLiteral("Processor"))));
    QVariantList fields{
        valueField(QStringLiteral("Vendor"), values.value(QStringLiteral("vendor_id")),
                   pathFor(QStringLiteral("/proc/cpuinfo"))),
        valueField(QStringLiteral("Model"), model, pathFor(QStringLiteral("/proc/cpuinfo"))),
        valueField(QStringLiteral("Family"), values.value(QStringLiteral("cpu family")),
                   pathFor(QStringLiteral("/proc/cpuinfo"))),
        valueField(QStringLiteral("Model number"), values.value(QStringLiteral("model")),
                   pathFor(QStringLiteral("/proc/cpuinfo"))),
        valueField(QStringLiteral("Stepping"), values.value(QStringLiteral("stepping")),
                   pathFor(QStringLiteral("/proc/cpuinfo"))),
        valueField(QStringLiteral("Microcode"), values.value(QStringLiteral("microcode")),
                   pathFor(QStringLiteral("/proc/cpuinfo"))),
        valueField(QStringLiteral("Flags"), values.value(QStringLiteral("flags"),
                                                         values.value(QStringLiteral("Features"))),
                   pathFor(QStringLiteral("/proc/cpuinfo"))),
        valueField(QStringLiteral("Logical CPUs"), cpuDirs.isEmpty() ? QString() : QString::number(cpuDirs.size()), cpuRoot),
        textField(QStringLiteral("Online CPUs"), cpuRoot + QStringLiteral("/online")),
        textField(QStringLiteral("Possible CPUs"), cpuRoot + QStringLiteral("/possible")),
        textField(QStringLiteral("Present CPUs"), cpuRoot + QStringLiteral("/present")),
    };

    const QStringList topologies = QDir(cpuRoot).entryList({QStringLiteral("cpu[0-9]*")}, QDir::Dirs,
                                                           QDir::Name);
    QSet<QString> packages;
    QSet<QString> cores;
    for (const QString &cpu : topologies) {
        const QString base = cpuRoot + QLatin1Char('/') + cpu + QStringLiteral("/topology");
        const QString package = readText(base + QStringLiteral("/physical_package_id")).value;
        const QString core = readText(base + QStringLiteral("/core_id")).value;
        bool packageOk = false, coreOk = false;
        const int packageId = package.toInt(&packageOk), coreId = core.toInt(&coreOk);
        if (packageOk && packageId >= 0) packages.insert(package);
        if (packageOk && packageId >= 0 && coreOk && coreId >= 0)
            cores.insert(package + QLatin1Char(':') + core);
        for (const QString &name : {QStringLiteral("physical_package_id"), QStringLiteral("core_id"),
                                    QStringLiteral("thread_siblings_list"), QStringLiteral("core_siblings_list"),
                                    QStringLiteral("die_id"), QStringLiteral("cluster_id")})
            fields.append(textField(cpu + QLatin1Char(' ') + name, base + QLatin1Char('/') + name));
    }
    fields.append(valueField(QStringLiteral("Packages"), packages.isEmpty() ? QString() : QString::number(packages.size()), cpuRoot));
    fields.append(valueField(QStringLiteral("Cores"), cores.isEmpty() ? QString() : QString::number(cores.size()), cpuRoot));

    QDir cpuDir(cpuRoot);
    const QStringList vulnerabilityDirs = cpuDir.entryList({QStringLiteral("vulnerabilities")}, QDir::Dirs);
    if (!vulnerabilityDirs.isEmpty()) {
        const QString vulnPath = cpuRoot + QStringLiteral("/vulnerabilities");
        const QStringList vulnerabilities = QDir(vulnPath).entryList(QDir::Files, QDir::Name);
        for (const QString &name : vulnerabilities)
            fields.append(textField(QStringLiteral("Vulnerability: ") + name,
                                    vulnPath + QLatin1Char('/') + name));
    }

    QSet<QString> caches;
    int cacheIndex = 0;
    for (const QString &cpu : topologies) {
        const QString cacheBase = cpuRoot + QLatin1Char('/') + cpu + QStringLiteral("/cache");
        for (const QString &cache : QDir(cacheBase).entryList({QStringLiteral("index*")}, QDir::Dirs, QDir::Name)) {
            const QString path = cacheBase + QLatin1Char('/') + cache;
            const QString level = readText(path + QStringLiteral("/level")).value;
            const QString type = readText(path + QStringLiteral("/type")).value;
            const QString size = readText(path + QStringLiteral("/size")).value;
            const QString shared = readText(path + QStringLiteral("/shared_cpu_list")).value;
            const QString identity = level + QLatin1Char('/') + type + QLatin1Char('/') + shared;
            if (level.isEmpty() || type.isEmpty() || size.isEmpty() || shared.isEmpty() || caches.contains(identity))
                continue;
            caches.insert(identity);
            const QString prefix = QStringLiteral("Cache %1 ").arg(++cacheIndex);
            for (const QString &name : {QStringLiteral("level"), QStringLiteral("type"), QStringLiteral("size"),
                                        QStringLiteral("shared_cpu_list"), QStringLiteral("coherency_line_size"),
                                        QStringLiteral("number_of_sets"), QStringLiteral("ways_of_associativity")})
                fields.append(textField(prefix + name, path + QLatin1Char('/') + name));
        }
    }

    const QString frequencyPath = cpuRoot + QStringLiteral("/cpu0/cpufreq");
    for (const QString &name : {QStringLiteral("scaling_driver"), QStringLiteral("scaling_governor"),
                                QStringLiteral("scaling_min_freq"), QStringLiteral("scaling_max_freq"),
                                QStringLiteral("cpuinfo_min_freq"), QStringLiteral("cpuinfo_max_freq")})
        fields.append(textField(name, frequencyPath + QLatin1Char('/') + name));

    snapshot.devices.append(device(QStringLiteral("cpu:system"), QStringLiteral("CPU"), model,
                                   QStringLiteral("%1 logical CPUs, %2 cores").arg(cpuDirs.size()).arg(cores.size()),
                                   QStringLiteral("/proc/cpuinfo, /sys/devices/system/cpu"), fields));
    snapshot.system.insert(QStringLiteral("cpu"), model.isEmpty() ? QStringLiteral("Unknown") : model);
    snapshot.system.insert(QStringLiteral("logicalCpus"), QString::number(cpuDirs.size()));
}

void Collector::appendMemory(InventorySnapshot &snapshot)
{
    const QString path = pathFor(QStringLiteral("/proc/meminfo"));
    const ReadResult info = readText(path);
    QMap<QString, ReadResult> results;
    QMap<QString, quint64> amounts;
    QMap<QString, QString> units;
    const QRegularExpression expression(QStringLiteral("^([^:]+):\\s*([0-9]+)(?:\\s+(kB))?\\s*$"));
    for (const QString &line : info.value.split(QLatin1Char('\n'))) {
        const QString key = line.section(QLatin1Char(':'), 0, 0).trimmed();
        if (!line.contains(QLatin1Char(':'))) continue;
        const auto match = expression.match(line);
        bool valid = false;
        const quint64 raw = match.captured(2).toULongLong(&valid);
        const bool bytes = match.captured(3) == QStringLiteral("kB");
        const bool countKey = key.startsWith(QStringLiteral("HugePages_"));
        const bool knownByteKey = key.startsWith(QStringLiteral("Mem")) || key.startsWith(QStringLiteral("Swap")) ||
            key == QStringLiteral("Buffers") || key == QStringLiteral("Cached") || key == QStringLiteral("CommitLimit") ||
            key == QStringLiteral("Committed_AS") || key == QStringLiteral("Hugepagesize");
        if ((countKey && bytes) || (knownByteKey && !bytes) || !match.hasMatch() || !valid || (bytes && raw > std::numeric_limits<quint64>::max() / 1024ULL)) {
            results.insert(key, {{}, ReadError});
            continue;
        }
        const quint64 amount = bytes ? raw * 1024ULL : raw;
        amounts.insert(key, amount);
        units.insert(key, bytes ? QStringLiteral("bytes") : QStringLiteral("count"));
        results.insert(key, {bytes ? prettyBytes(amount) : QString::number(amount), Available});
    }
    const QStringList keys{QStringLiteral("MemTotal"), QStringLiteral("MemAvailable"),
                           QStringLiteral("MemFree"), QStringLiteral("Buffers"), QStringLiteral("Cached"),
                           QStringLiteral("SwapTotal"), QStringLiteral("SwapFree"), QStringLiteral("CommitLimit"),
                           QStringLiteral("Committed_AS"), QStringLiteral("HugePages_Total"),
                           QStringLiteral("HugePages_Free"), QStringLiteral("HugePages_Rsvd"),
                           QStringLiteral("HugePages_Surp"), QStringLiteral("Hugepagesize")};
    QVariantList fields;
    for (const QString &key : keys) {
        auto item = field(key, results.value(key, {{}, info.status == Available ? Unavailable : info.status}), path);
        if (amounts.contains(key)) {
            item.insert(QStringLiteral("rawValue"), QVariant::fromValue(amounts.value(key)));
            item.insert(QStringLiteral("unit"), units.value(key));
        }
        fields.append(item);
    }
    const bool hasTotal = amounts.contains(QStringLiteral("MemTotal")) && units.value(QStringLiteral("MemTotal")) == QStringLiteral("bytes");
    const quint64 total = amounts.value(QStringLiteral("MemTotal"));
    const bool canCalculateUsed = hasTotal && amounts.contains(QStringLiteral("MemAvailable")) &&
            units.value(QStringLiteral("MemAvailable")) == QStringLiteral("bytes") && amounts.value(QStringLiteral("MemAvailable")) <= total;
    fields.append(valueField(QStringLiteral("Used memory"), canCalculateUsed ? prettyBytes(total - amounts.value(QStringLiteral("MemAvailable"))) : QString(), path));
    const QString nodeRoot = pathFor(QStringLiteral("/sys/devices/system/node"));
    for (const QString &node : QDir(nodeRoot).entryList({QStringLiteral("node[0-9]*")}, QDir::Dirs, QDir::Name)) {
        const QString base = nodeRoot + QLatin1Char('/') + node;
        for (const QString &key : {QStringLiteral("cpulist"), QStringLiteral("cpumap"), QStringLiteral("meminfo"), QStringLiteral("distance")})
            fields.append(textField(node + QLatin1Char(' ') + key, base + QLatin1Char('/') + key));
    }
    auto memory = device(QStringLiteral("memory:system"), QStringLiteral("内存"),
                         QStringLiteral("Physical memory"), hasTotal ? prettyBytes(total) : QString(), path, fields);
    memory.insert(QStringLiteral("status"), hasTotal ? Available : results.value(QStringLiteral("MemTotal"), {{}, info.status}).status);
    snapshot.devices.append(memory);
    snapshot.system.insert(QStringLiteral("memoryTotal"), hasTotal ? prettyBytes(total) : QStringLiteral("Unknown"));
}

} // namespace neko::inventory
