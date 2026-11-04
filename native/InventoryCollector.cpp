#include "InventoryCollector.h"

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

namespace neko {
namespace {

const QString Available = QStringLiteral("available");
const QString PermissionDenied = QStringLiteral("permission-denied");
const QString Unavailable = QStringLiteral("unavailable");

struct ReadResult {
    QString value;
    QString status = Unavailable;
};

ReadResult readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QFileInfo info(path);
        return {{}, info.exists() ? PermissionDenied : Unavailable};
    }
    QTextStream stream(&file);
    const QString value = stream.readAll().trimmed();
    return value.isEmpty() ? ReadResult{{}, Unavailable} : ReadResult{value, Available};
}

QString cleanId(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.:-]+")), QStringLiteral("_"));
    return value;
}

QVariantMap field(const QString &key, const ReadResult &result, const QString &source,
                  bool sensitive = false)
{
    return {{QStringLiteral("key"), key},
            {QStringLiteral("value"), result.value},
            {QStringLiteral("source"), source},
            {QStringLiteral("status"), result.status},
            {QStringLiteral("sensitive"), sensitive}};
}

QVariantMap textField(const QString &key, const QString &path, bool sensitive = false)
{
    return field(key, readText(path), path, sensitive);
}

QVariantMap valueField(const QString &key, const QString &value, const QString &source,
                       bool sensitive = false)
{
    return field(key, value.isEmpty() ? ReadResult{} : ReadResult{value, Available}, source, sensitive);
}

QVariantMap device(const QString &id, const QString &category, const QString &name,
                   const QString &summary, const QString &source, const QVariantList &fields)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("category"), category},
            {QStringLiteral("name"), name.isEmpty() ? QStringLiteral("Unknown") : name},
            {QStringLiteral("summary"), summary},
            {QStringLiteral("source"), source},
            {QStringLiteral("status"), name.isEmpty() ? Unavailable : Available},
            {QStringLiteral("fields"), fields}};
}

QVariantList directoryFields(const QString &directory, const QStringList &names,
                             const QSet<QString> &sensitive = {})
{
    QVariantList result;
    for (const QString &name : names)
        result.append(textField(name, directory + QLatin1Char('/') + name, sensitive.contains(name)));
    return result;
}

QString prettyBytes(quint64 bytes)
{
    static const QStringList units{QStringLiteral("B"), QStringLiteral("KiB"),
                                   QStringLiteral("MiB"), QStringLiteral("GiB"),
                                   QStringLiteral("TiB"), QStringLiteral("PiB")};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', unit == 0 ? 0 : 2), units.at(unit));
}

QString osPrettyName()
{
    const ReadResult osRelease = readText(QStringLiteral("/etc/os-release"));
    const QStringList lines = osRelease.value.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("PRETTY_NAME="))) {
            QString value = line.mid(12).trimmed();
            if (value.size() >= 2 && ((value.startsWith('"') && value.endsWith('"')) ||
                                      (value.startsWith('\'') && value.endsWith('\''))))
                value = value.mid(1, value.size() - 2);
            return value;
        }
    }
    return QSysInfo::prettyProductName();
}

void appendCpu(InventorySnapshot &snapshot)
{
    const ReadResult cpuInfoResult = readText(QStringLiteral("/proc/cpuinfo"));
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

    const QString cpuRoot = QStringLiteral("/sys/devices/system/cpu");
    const QStringList cpuDirs = QDir(cpuRoot).entryList({QStringLiteral("cpu[0-9]*")}, QDir::Dirs,
                                                        QDir::Name);
    const QString model = values.value(QStringLiteral("model name"),
                                       values.value(QStringLiteral("Hardware"),
                                                    values.value(QStringLiteral("Processor"))));
    QVariantList fields{
        valueField(QStringLiteral("Vendor"), values.value(QStringLiteral("vendor_id")),
                   QStringLiteral("/proc/cpuinfo")),
        valueField(QStringLiteral("Model"), model, QStringLiteral("/proc/cpuinfo")),
        valueField(QStringLiteral("Family"), values.value(QStringLiteral("cpu family")),
                   QStringLiteral("/proc/cpuinfo")),
        valueField(QStringLiteral("Model number"), values.value(QStringLiteral("model")),
                   QStringLiteral("/proc/cpuinfo")),
        valueField(QStringLiteral("Stepping"), values.value(QStringLiteral("stepping")),
                   QStringLiteral("/proc/cpuinfo")),
        valueField(QStringLiteral("Microcode"), values.value(QStringLiteral("microcode")),
                   QStringLiteral("/proc/cpuinfo")),
        valueField(QStringLiteral("Flags"), values.value(QStringLiteral("flags"),
                                                         values.value(QStringLiteral("Features"))),
                   QStringLiteral("/proc/cpuinfo")),
        valueField(QStringLiteral("Logical CPUs"), QString::number(cpuDirs.size()), cpuRoot),
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
        packages.insert(readText(base + QStringLiteral("/physical_package_id")).value);
        cores.insert(readText(base + QStringLiteral("/core_id")).value + QLatin1Char(':') +
                     readText(base + QStringLiteral("/physical_package_id")).value);
    }
    fields.append(valueField(QStringLiteral("Packages"), QString::number(packages.size()), cpuRoot));
    fields.append(valueField(QStringLiteral("Cores"), QString::number(cores.size()), cpuRoot));

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
    for (const QString &cpu : topologies) {
        const QString cacheBase = cpuRoot + QLatin1Char('/') + cpu + QStringLiteral("/cache");
        for (const QString &cache : QDir(cacheBase).entryList({QStringLiteral("index*")}, QDir::Dirs)) {
            const QString path = cacheBase + QLatin1Char('/') + cache;
            caches.insert(readText(path + QStringLiteral("/level")).value + QStringLiteral("/") +
                          readText(path + QStringLiteral("/type")).value + QStringLiteral("/") +
                          readText(path + QStringLiteral("/size")).value + QStringLiteral("/") +
                          readText(path + QStringLiteral("/shared_cpu_list")).value);
        }
    }
    int cacheIndex = 0;
    for (const QString &cache : caches)
        fields.append(valueField(QStringLiteral("Cache topology ") + QString::number(++cacheIndex),
                                 cache, cpuRoot + QStringLiteral("/cpu*/cache")));

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

void appendMemory(InventorySnapshot &snapshot)
{
    const QString path = QStringLiteral("/proc/meminfo");
    const ReadResult info = readText(path);
    QVariantMap values;
    const QRegularExpression expression(QStringLiteral("^([^:]+):\\s*(\\d+)(?:\\s+(kB))?"));
    for (const QString &line : info.value.split(QLatin1Char('\n'))) {
        const auto match = expression.match(line);
        if (match.hasMatch()) {
            const quint64 amount = match.captured(2).toULongLong() *
                                   (match.captured(3) == QStringLiteral("kB") ? 1024ULL : 1ULL);
            values.insert(match.captured(1), QVariant::fromValue(amount));
        }
    }
    const quint64 total = values.value(QStringLiteral("MemTotal")).toULongLong();
    QVariantList fields;
    const QStringList memoryKeys{QStringLiteral("MemTotal"), QStringLiteral("MemAvailable"),
                                 QStringLiteral("MemFree"), QStringLiteral("Buffers"),
                                 QStringLiteral("Cached"), QStringLiteral("SwapTotal"),
                                 QStringLiteral("SwapFree"), QStringLiteral("CommitLimit"),
                                 QStringLiteral("Committed_AS"), QStringLiteral("HugePages_Total"),
                                 QStringLiteral("HugePages_Free"), QStringLiteral("Hugepagesize")};
    for (const QString &key : memoryKeys) {
        const quint64 bytes = values.value(key).toULongLong();
        fields.append(valueField(key, bytes == 0 && !values.contains(key) ? QString() : prettyBytes(bytes), path));
    }
    const QString nodeRoot = QStringLiteral("/sys/devices/system/node");
    const QStringList nodes = QDir(nodeRoot).entryList({QStringLiteral("node[0-9]*")}, QDir::Dirs, QDir::Name);
    for (const QString &node : nodes) {
        const QString base = nodeRoot + QLatin1Char('/') + node;
        fields.append(textField(node + QStringLiteral(" CPU list"), base + QStringLiteral("/cpulist")));
        fields.append(textField(node + QStringLiteral(" memory"), base + QStringLiteral("/meminfo")));
    }
    snapshot.devices.append(device(QStringLiteral("memory:system"), QStringLiteral("内存"),
                                   QStringLiteral("Physical memory"), prettyBytes(total), path, fields));
    snapshot.system.insert(QStringLiteral("memoryTotal"), total ? prettyBytes(total) : QStringLiteral("Unknown"));
}

void appendDmi(InventorySnapshot &snapshot)
{
    const QString base = QStringLiteral("/sys/class/dmi/id");
    const QStringList keys{QStringLiteral("sys_vendor"), QStringLiteral("product_name"),
                           QStringLiteral("product_version"), QStringLiteral("product_family"),
                           QStringLiteral("product_sku"), QStringLiteral("board_vendor"),
                           QStringLiteral("board_name"), QStringLiteral("board_version"),
                           QStringLiteral("bios_vendor"), QStringLiteral("bios_version"),
                           QStringLiteral("bios_date"), QStringLiteral("bios_release"),
                           QStringLiteral("chassis_vendor"), QStringLiteral("chassis_type"),
                           QStringLiteral("chassis_version"), QStringLiteral("modalias"),
                           QStringLiteral("board_serial"), QStringLiteral("product_serial"),
                           QStringLiteral("chassis_serial"), QStringLiteral("product_uuid"),
                           QStringLiteral("board_asset_tag"), QStringLiteral("chassis_asset_tag")};
    const QSet<QString> sensitive{QStringLiteral("board_serial"), QStringLiteral("product_serial"),
                                  QStringLiteral("chassis_serial"), QStringLiteral("product_uuid"),
                                  QStringLiteral("board_asset_tag"), QStringLiteral("chassis_asset_tag")};
    const QVariantList fields = directoryFields(base, keys, sensitive);
    const QString name = readText(base + QStringLiteral("/product_name")).value;
    const QString vendor = readText(base + QStringLiteral("/sys_vendor")).value;
    snapshot.devices.append(device(QStringLiteral("system:board"), QStringLiteral("主板/BIOS"),
                                   (vendor + QLatin1Char(' ') + name).trimmed(),
                                   QStringLiteral("SMBIOS via kernel DMI sysfs"), base, fields));
}

void appendStorage(InventorySnapshot &snapshot)
{
    const QString root = QStringLiteral("/sys/class/block");
    const QStringList entries = QDir(root).entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                     QDir::Name);
    for (const QString &entry : entries) {
        const QString path = root + QLatin1Char('/') + entry;
        const QString realPath = QFileInfo(path).canonicalFilePath();
        if (entry.startsWith(QStringLiteral("loop")) || entry.startsWith(QStringLiteral("ram")) ||
            entry.startsWith(QStringLiteral("zram")))
            continue;
        QVariantList fields = directoryFields(path, {
            QStringLiteral("dev"), QStringLiteral("size"), QStringLiteral("ro"),
            QStringLiteral("removable"), QStringLiteral("stat"), QStringLiteral("diskseq"),
            QStringLiteral("queue/logical_block_size"), QStringLiteral("queue/physical_block_size"),
            QStringLiteral("queue/minimum_io_size"), QStringLiteral("queue/optimal_io_size"),
            QStringLiteral("queue/rotational"), QStringLiteral("queue/nr_requests"),
            QStringLiteral("queue/scheduler"), QStringLiteral("queue/write_cache"),
            QStringLiteral("queue/discard_granularity"), QStringLiteral("queue/discard_max_bytes")});
        const QString sectors = readText(path + QStringLiteral("/size")).value;
        if (!sectors.isEmpty())
            fields.append(valueField(QStringLiteral("Capacity"), prettyBytes(sectors.toULongLong() * 512ULL), path));
        const QString devicePath = realPath + QStringLiteral("/device");
        for (const QString &key : {QStringLiteral("model"), QStringLiteral("vendor"),
                                   QStringLiteral("rev"), QStringLiteral("serial")})
            fields.append(textField(key, devicePath + QLatin1Char('/') + key, key == QStringLiteral("serial")));
        fields.append(valueField(QStringLiteral("SMART / health"),
                                 QStringLiteral("Not queried: use UDisks2 or libnvme; permissions may restrict direct access"),
                                 QStringLiteral("UDisks2 D-Bus / libnvme")));

        const QStorageInfo storage(path);
        if (storage.isValid()) {
            fields.append(valueField(QStringLiteral("Mount point"), storage.rootPath(), QStringLiteral("QStorageInfo")));
            fields.append(valueField(QStringLiteral("Filesystem"), QString::fromLatin1(storage.fileSystemType()), QStringLiteral("QStorageInfo")));
            if (storage.isReady()) {
                fields.append(valueField(QStringLiteral("Available space"), prettyBytes(storage.bytesAvailable()), QStringLiteral("QStorageInfo")));
                fields.append(valueField(QStringLiteral("Total filesystem space"), prettyBytes(storage.bytesTotal()), QStringLiteral("QStorageInfo")));
            }
        }
        const QString model = readText(devicePath + QStringLiteral("/model")).value;
        const QString name = model.isEmpty() ? entry : model;
        snapshot.devices.append(device(QStringLiteral("storage:") + entry, QStringLiteral("存储"), name,
                                       QStringLiteral("Block device %1").arg(entry), realPath.isEmpty() ? path : realPath,
                                       fields));
    }
}

void appendPci(InventorySnapshot &snapshot)
{
    const QString root = QStringLiteral("/sys/bus/pci/devices");
    const QStringList entries = QDir(root).entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                     QDir::Name);
    for (const QString &entry : entries) {
        const QString path = root + QLatin1Char('/') + entry;
        const QString classCode = readText(path + QStringLiteral("/class")).value;
        const int classValue = classCode.toUInt(nullptr, 0);
        QString category = QStringLiteral("PCI");
        if (((classValue >> 16) & 0xff) == 0x03)
            category = QStringLiteral("显卡");
        else if (((classValue >> 16) & 0xff) == 0x04)
            category = QStringLiteral("音频");
        QVariantList fields = directoryFields(path, {
            QStringLiteral("vendor"), QStringLiteral("device"), QStringLiteral("subsystem_vendor"),
            QStringLiteral("subsystem_device"), QStringLiteral("class"), QStringLiteral("revision"),
            QStringLiteral("irq"), QStringLiteral("numa_node"), QStringLiteral("local_cpulist"),
            QStringLiteral("enable"), QStringLiteral("current_link_speed"),
            QStringLiteral("current_link_width"), QStringLiteral("max_link_speed"),
            QStringLiteral("max_link_width"), QStringLiteral("consistent_dma_mask_bits"),
            QStringLiteral("dma_mask_bits"), QStringLiteral("modalias")});
        const QString driver = QFileInfo(path + QStringLiteral("/driver")).symLinkTarget();
        fields.append(valueField(QStringLiteral("Driver"), driver.isEmpty() ? QString() : QFileInfo(driver).fileName(),
                                 path + QStringLiteral("/driver")));
        fields.append(valueField(QStringLiteral("IOMMU group"),
                                 QFileInfo(path + QStringLiteral("/iommu_group")).symLinkTarget(),
                                 path + QStringLiteral("/iommu_group")));
        const QString vendor = readText(path + QStringLiteral("/vendor")).value;
        const QString product = readText(path + QStringLiteral("/device")).value;
        snapshot.devices.append(device(QStringLiteral("pci:") + entry, category,
                                       QStringLiteral("PCI device %1").arg(entry),
                                       QStringLiteral("%1 %2 class %3").arg(vendor, product, classCode),
                                       path, fields));
    }
}

void appendUsb(InventorySnapshot &snapshot)
{
    const QString root = QStringLiteral("/sys/bus/usb/devices");
    for (const QString &entry : QDir(root).entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                     QDir::Name)) {
        const QString path = root + QLatin1Char('/') + entry;
        const ReadResult vendor = readText(path + QStringLiteral("/idVendor"));
        const ReadResult product = readText(path + QStringLiteral("/idProduct"));
        if (vendor.status == Unavailable && product.status == Unavailable)
            continue;
        QVariantList fields = directoryFields(path, {
            QStringLiteral("idVendor"), QStringLiteral("idProduct"), QStringLiteral("manufacturer"),
            QStringLiteral("product"), QStringLiteral("bcdDevice"), QStringLiteral("bDeviceClass"),
            QStringLiteral("bDeviceSubClass"), QStringLiteral("bDeviceProtocol"),
            QStringLiteral("bMaxPower"), QStringLiteral("speed"), QStringLiteral("busnum"),
            QStringLiteral("devnum"), QStringLiteral("devpath"), QStringLiteral("version"),
            QStringLiteral("serial")}, {QStringLiteral("serial")});
        const QString name = readText(path + QStringLiteral("/product")).value;
        snapshot.devices.append(device(QStringLiteral("usb:") + cleanId(entry), QStringLiteral("USB"), name,
                                       QStringLiteral("%1:%2").arg(vendor.value, product.value), path, fields));
    }
}

void appendNetwork(InventorySnapshot &snapshot)
{
    const QString root = QStringLiteral("/sys/class/net");
    for (const QString &entry : QDir(root).entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                     QDir::Name)) {
        const QString path = root + QLatin1Char('/') + entry;
        const QString realPath = QFileInfo(path).canonicalFilePath();
        QVariantList fields = directoryFields(path, {
            QStringLiteral("address"), QStringLiteral("addr_assign_type"), QStringLiteral("type"),
            QStringLiteral("ifindex"), QStringLiteral("iflink"), QStringLiteral("operstate"),
            QStringLiteral("carrier"), QStringLiteral("mtu"), QStringLiteral("speed"),
            QStringLiteral("duplex"), QStringLiteral("dormant"), QStringLiteral("statistics/rx_bytes"),
            QStringLiteral("statistics/tx_bytes"), QStringLiteral("statistics/rx_packets"),
            QStringLiteral("statistics/tx_packets"), QStringLiteral("statistics/rx_errors"),
            QStringLiteral("statistics/tx_errors"), QStringLiteral("statistics/rx_dropped"),
            QStringLiteral("statistics/tx_dropped")}, {QStringLiteral("address")});
        const QString driver = QFileInfo(realPath + QStringLiteral("/device/driver")).symLinkTarget();
        fields.append(valueField(QStringLiteral("Driver"), driver.isEmpty() ? QString() : QFileInfo(driver).fileName(),
                                 realPath + QStringLiteral("/device/driver")));
        const QString pathName = QFileInfo(realPath).fileName();
        snapshot.devices.append(device(QStringLiteral("net:") + entry, QStringLiteral("网络"), entry,
                                       QStringLiteral("%1 (%2)").arg(readText(path + QStringLiteral("/operstate")).value,
                                                                      readText(path + QStringLiteral("/speed")).value),
                                       path, fields));
    }
}

void appendPower(InventorySnapshot &snapshot)
{
    const QString root = QStringLiteral("/sys/class/power_supply");
    for (const QString &entry : QDir(root).entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                     QDir::Name)) {
        const QString path = root + QLatin1Char('/') + entry;
        const QString type = readText(path + QStringLiteral("/type")).value;
        QVariantList fields = directoryFields(path, {
            QStringLiteral("type"), QStringLiteral("scope"), QStringLiteral("present"),
            QStringLiteral("online"), QStringLiteral("status"), QStringLiteral("health"),
            QStringLiteral("capacity"), QStringLiteral("capacity_level"), QStringLiteral("voltage_now"),
            QStringLiteral("voltage_min_design"), QStringLiteral("voltage_max_design"),
            QStringLiteral("current_now"), QStringLiteral("current_max"), QStringLiteral("power_now"),
            QStringLiteral("energy_now"), QStringLiteral("energy_full"),
            QStringLiteral("energy_full_design"), QStringLiteral("charge_now"),
            QStringLiteral("charge_full"), QStringLiteral("charge_full_design"),
            QStringLiteral("cycle_count"), QStringLiteral("temp"), QStringLiteral("manufacturer"),
            QStringLiteral("model_name"), QStringLiteral("technology"), QStringLiteral("serial_number")},
            {QStringLiteral("serial_number")});
        const QString name = readText(path + QStringLiteral("/model_name")).value;
        snapshot.devices.append(device(QStringLiteral("power:") + entry,
                                       type == QStringLiteral("Battery") ? QStringLiteral("电池") : QStringLiteral("电源"),
                                       name.isEmpty() ? entry : name,
                                       type + QStringLiteral(" power_supply"), path, fields));
    }
}

void appendDisplays(InventorySnapshot &snapshot)
{
    const QString root = QStringLiteral("/sys/class/drm");
    const QStringList entries = QDir(root).entryList({QStringLiteral("card*-*")}, QDir::Dirs,
                                                     QDir::Name);
    for (const QString &entry : entries) {
        const QString path = root + QLatin1Char('/') + entry;
        const QString status = readText(path + QStringLiteral("/status")).value;
        const QString edidPath = path + QStringLiteral("/edid");
        QFile edidFile(edidPath);
        QVariantList fields;
        fields.append(textField(QStringLiteral("Connection status"), path + QStringLiteral("/status")));
        for (const QString &key : {QStringLiteral("enabled"), QStringLiteral("dpms"),
                                   QStringLiteral("modes"), QStringLiteral("subconnector"),
                                   QStringLiteral("connector_id"), QStringLiteral("ddc"),
                                   QStringLiteral("link_status"), QStringLiteral("max bpc")})
            fields.append(textField(key, path + QLatin1Char('/') + key));
        if (edidFile.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = edidFile.readAll();
            fields.append(valueField(QStringLiteral("EDID raw (hex)"), QString::fromLatin1(bytes.toHex(' ')),
                                     edidPath, true));
            fields.append(valueField(QStringLiteral("EDID bytes"), QString::number(bytes.size()), edidPath));
        } else {
            const QFileInfo info(edidPath);
            fields.append(field(QStringLiteral("EDID raw (hex)"),
                                {{}, info.exists() ? PermissionDenied : Unavailable}, edidPath, true));
        }
        const QString pci = QFileInfo(path + QStringLiteral("/device")).canonicalFilePath();
        snapshot.devices.append(device(QStringLiteral("display:") + entry, QStringLiteral("显示器"), entry,
                                       status.isEmpty() ? QStringLiteral("DRM connector") : status,
                                       path, fields));
        if (!pci.isEmpty()) {
            const QString vendor = readText(pci + QStringLiteral("/vendor")).value;
            const QString product = readText(pci + QStringLiteral("/device")).value;
            if (!vendor.isEmpty() || !product.isEmpty()) {
                auto &last = snapshot.devices.last();
                QVariantMap display = last.toMap();
                display.insert(QStringLiteral("summary"), QStringLiteral("%1 (%2 %3)").arg(
                                   status, vendor, product).trimmed());
                snapshot.devices.last() = display;
            }
        }
    }
}

void appendAudioCards(InventorySnapshot &snapshot)
{
    const QString root = QStringLiteral("/proc/asound");
    const ReadResult cards = readText(root + QStringLiteral("/cards"));
    if (cards.status == Available) {
        int index = 0;
        const QStringList lines = cards.value.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const QRegularExpression match(QStringLiteral("^\\s*(\\d+) \\[([^]]+)\\] : (.*)$"));
            const auto result = match.match(line);
            if (!result.hasMatch())
                continue;
            const QString card = result.captured(1);
            const QString id = result.captured(2);
            const QString name = result.captured(3);
            QVariantList fields{valueField(QStringLiteral("Card index"), card, root + QStringLiteral("/cards")),
                                valueField(QStringLiteral("ALSA id"), id, root + QStringLiteral("/cards")),
                                textField(QStringLiteral("Codec details"), root + QStringLiteral("/card") + card + QStringLiteral("/codec#0"))};
            snapshot.devices.append(device(QStringLiteral("audio:") + card, QStringLiteral("音频"), name,
                                           id, root + QStringLiteral("/cards"), fields));
            ++index;
        }
        Q_UNUSED(index)
    }
}

QString uptimeText()
{
    const QString raw = readText(QStringLiteral("/proc/uptime")).value.section(QLatin1Char(' '), 0, 0);
    bool ok = false;
    const quint64 seconds = raw.toDouble(&ok);
    if (!ok)
        return {};
    const quint64 days = seconds / 86400;
    const quint64 hours = (seconds % 86400) / 3600;
    const quint64 minutes = (seconds % 3600) / 60;
    return days ? QStringLiteral("%1 d %2 h %3 min").arg(days).arg(hours).arg(minutes)
                : QStringLiteral("%1 h %2 min").arg(hours).arg(minutes);
}

} // namespace

InventorySnapshot collectInventory()
{
    InventorySnapshot snapshot;
    snapshot.system.insert(QStringLiteral("os"), osPrettyName());
    snapshot.system.insert(QStringLiteral("kernel"), QSysInfo::kernelVersion());
    snapshot.system.insert(QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture());
    snapshot.system.insert(QStringLiteral("hostname"), QSysInfo::machineHostName());
    snapshot.system.insert(QStringLiteral("uptime"), uptimeText());

    appendCpu(snapshot);
    appendMemory(snapshot);
    appendDmi(snapshot);
    appendStorage(snapshot);
    appendPci(snapshot);
    appendUsb(snapshot);
    appendNetwork(snapshot);
    appendPower(snapshot);
    appendDisplays(snapshot);
    appendAudioCards(snapshot);
    return snapshot;
}

} // namespace neko
