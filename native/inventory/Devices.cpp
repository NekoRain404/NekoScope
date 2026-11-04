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
#ifdef NEKO_HAS_DISPLAY_INFO
extern "C" {
#include <libdisplay-info/info.h>
}
#include <cstdlib>
#include <memory>
#endif


namespace neko::inventory {

void Collector::appendDmi(InventorySnapshot &snapshot)
{
    const QString base = pathFor(QStringLiteral("/sys/class/dmi/id"));
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

void Collector::appendStorage(InventorySnapshot &snapshot)
{
    const QString root = pathFor(QStringLiteral("/sys/class/block"));
    const QString mountInfoPath = pathFor(QStringLiteral("/proc/self/mountinfo"));
    const QStringList mounts = readText(mountInfoPath).value.split(QLatin1Char('\n'));
    const auto unescape = [](QString value) {
        value.replace(QStringLiteral("\\040"), QStringLiteral(" "));
        value.replace(QStringLiteral("\\011"), QStringLiteral("\t"));
        value.replace(QStringLiteral("\\012"), QStringLiteral("\n"));
        value.replace(QStringLiteral("\\134"), QString(QChar(0x5c)));
        return value;
    };
    for (const QString &entry : QDir(root).entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString path = root + QLatin1Char('/') + entry;
        const QString realPath = QFileInfo(path).canonicalFilePath();
        if (entry.startsWith(QStringLiteral("loop")) || entry.startsWith(QStringLiteral("ram")) || entry.startsWith(QStringLiteral("zram")))
            continue;
        QVariantList fields = directoryFields(path, {
            QStringLiteral("dev"), QStringLiteral("size"), QStringLiteral("ro"), QStringLiteral("removable"),
            QStringLiteral("stat"), QStringLiteral("diskseq"), QStringLiteral("partition"),
            QStringLiteral("queue/logical_block_size"), QStringLiteral("queue/physical_block_size"),
            QStringLiteral("queue/rotational"), QStringLiteral("queue/scheduler"),
            QStringLiteral("queue/write_cache"), QStringLiteral("queue/discard_max_bytes")});
        const auto sectors = readText(path + QStringLiteral("/size"));
        bool validSectors = false;
        const quint64 sectorCount = sectors.value.toULongLong(&validSectors);
        auto capacity = field(QStringLiteral("Capacity"),
                              sectors.status != Available ? sectors :
                              validSectors && sectorCount <= std::numeric_limits<quint64>::max() / 512ULL
                                ? ReadResult{prettyBytes(sectorCount * 512ULL), Available} : ReadResult{{}, ReadError},
                              path + QStringLiteral("/size"));
        if (validSectors && sectorCount <= std::numeric_limits<quint64>::max() / 512ULL) {
            capacity.insert(QStringLiteral("rawValue"), QVariant::fromValue(sectorCount * 512ULL));
            capacity.insert(QStringLiteral("unit"), QStringLiteral("bytes"));
        }
        fields.append(capacity);
        const QString devicePath = path + QStringLiteral("/device");
        for (const QString &key : {QStringLiteral("model"), QStringLiteral("vendor"), QStringLiteral("rev"), QStringLiteral("serial")})
            fields.append(textField(key, devicePath + QLatin1Char('/') + key, key == QStringLiteral("serial")));
        auto health = field(QStringLiteral("SMART / health"), {{}, Unavailable}, QStringLiteral("UDisks2 D-Bus / libnvme"));
        health.insert(QStringLiteral("reason"), QStringLiteral("SMART backend not implemented; no health query performed"));
        fields.append(health);
        const QString dev = readText(path + QStringLiteral("/dev")).value;
        int mountIndex = 0;
        for (const QString &line : mounts) {
            const QStringList columns = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            const qsizetype separator = columns.indexOf(QStringLiteral("-"));
            if (columns.size() < 10 || separator < 6 || separator + 3 >= columns.size() || dev.isEmpty() || columns.at(2) != dev)
                continue;
            const QString mount = unescape(columns.at(4));
            const QString suffix = mountIndex++ == 0 ? QString() : QStringLiteral(" %1").arg(mountIndex);
            fields.append(valueField(QStringLiteral("Mount point") + suffix, mount, mountInfoPath, true));
            fields.append(valueField(QStringLiteral("Filesystem") + suffix, columns.at(separator + 1), mountInfoPath));
            fields.append(valueField(QStringLiteral("Mount options") + suffix, columns.at(5), mountInfoPath));
            // Alternate namespace roots must never stat a host filesystem as fixture data.
            if (source.root.isEmpty()) {
                const QStorageInfo storage(mount);
                if (storage.isValid() && storage.isReady() && storage.rootPath() == mount) {
                    fields.append(valueField(QStringLiteral("Available space") + suffix, prettyBytes(static_cast<quint64>(std::max<qint64>(0, storage.bytesAvailable()))), QStringLiteral("statvfs:") + mount));
                    fields.append(valueField(QStringLiteral("Total filesystem space") + suffix, prettyBytes(static_cast<quint64>(std::max<qint64>(0, storage.bytesTotal()))), QStringLiteral("statvfs:") + mount));
                }
            }
        }
        const QString model = readText(devicePath + QStringLiteral("/model")).value;
        snapshot.devices.append(device(QStringLiteral("storage:") + entry, QStringLiteral("存储"), model.isEmpty() ? entry : model,
                                       QStringLiteral("Block device %1").arg(entry), realPath.isEmpty() ? path : realPath, fields));
    }
}

void Collector::appendPci(InventorySnapshot &snapshot)
{
    const QString root = pathFor(QStringLiteral("/sys/bus/pci/devices"));
    QMap<QString, QString> vendors, products;
    QString databasePath;
    for (const QString &candidate : {QStringLiteral("/usr/share/hwdata/pci.ids"), QStringLiteral("/usr/share/misc/pci.ids"), QStringLiteral("/usr/share/pci.ids")}) {
        const auto content = readText(pathFor(candidate));
        if (content.status != Available) continue;
        databasePath = pathFor(candidate);
        QString currentVendor;
        const QRegularExpression vendorLine(QStringLiteral("^([0-9a-fA-F]{4})  (.+)$"));
        const QRegularExpression productLine(QStringLiteral("^\\t([0-9a-fA-F]{4})  (.+)$"));
        for (const auto &line : content.value.split(QLatin1Char('\n'))) {
            const auto vendor = vendorLine.match(line);
            const auto product = productLine.match(line);
            if (vendor.hasMatch()) {
                currentVendor = vendor.captured(1).toLower();
                vendors.insert(currentVendor, vendor.captured(2).trimmed());
            } else if (product.hasMatch() && !currentVendor.isEmpty()) {
                products.insert(currentVendor + QLatin1Char(':') + product.captured(1).toLower(), product.captured(2).trimmed());
            } else if (!line.isEmpty() && !line.startsWith(QLatin1Char('\t')) && !line.startsWith(QLatin1Char('#'))) {
                currentVendor.clear();
            }
        }
        break;
    }
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
        const QString vendorId = QStringLiteral("%1").arg(vendor.toUInt(nullptr, 0), 4, 16, QLatin1Char('0'));
        const QString productId = QStringLiteral("%1").arg(product.toUInt(nullptr, 0), 4, 16, QLatin1Char('0'));
        const QString vendorName = vendors.value(vendorId);
        const QString productName = products.value(vendorId + QLatin1Char(':') + productId);
        QString resolvedName = (vendorName + QLatin1Char(' ') + productName).trimmed();
        if (resolvedName.isEmpty()) resolvedName = QStringLiteral("PCI device %1").arg(entry);
        if (!databasePath.isEmpty()) {
            fields.append(valueField(QStringLiteral("Vendor name"), vendorName, databasePath));
            fields.append(valueField(QStringLiteral("Device name"), productName, databasePath));
        }
        snapshot.devices.append(device(QStringLiteral("pci:") + entry, category,
                                       resolvedName,
                                       QStringLiteral("%1 %2 class %3").arg(vendor, product, classCode),
                                       path, fields));
    }
}

void Collector::appendUsb(InventorySnapshot &snapshot)
{
    const QString root = pathFor(QStringLiteral("/sys/bus/usb/devices"));
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

void Collector::appendNetwork(InventorySnapshot &snapshot)
{
    const QString root = pathFor(QStringLiteral("/sys/class/net"));
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
                snapshot.devices.append(device(QStringLiteral("net:") + entry, QStringLiteral("网络"), entry,
                                       QStringLiteral("%1 (%2)").arg(readText(path + QStringLiteral("/operstate")).value,
                                                                      readText(path + QStringLiteral("/speed")).value),
                                       path, fields));
    }
}

void Collector::appendPower(InventorySnapshot &snapshot)
{
    const QString root = pathFor(QStringLiteral("/sys/class/power_supply"));
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

void Collector::appendDisplays(InventorySnapshot &snapshot)
{
    const QString root = pathFor(QStringLiteral("/sys/class/drm"));
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
        QString displayName;
        if (edidFile.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = edidFile.read(32768);
            const auto byte = [&bytes](qsizetype offset) { return static_cast<unsigned char>(bytes.at(offset)); };
            bool valid = bytes.size() >= 128 && bytes.first(8) == QByteArray::fromHex("00ffffffffffff00");
            const qsizetype blockCount = valid ? 1 + byte(126) : 0;
            valid = valid && bytes.size() >= blockCount * 128;
            for (qsizetype block = 0; valid && block < blockCount; ++block) {
                unsigned sum = 0;
                for (qsizetype index = 0; index < 128; ++index) sum += byte(block * 128 + index);
                valid = sum % 256 == 0;
            }
            fields.append(field(QStringLiteral("EDID validation"), {valid ? QStringLiteral("Valid header and block checksums") : QStringLiteral("Invalid or truncated EDID"), valid ? Available : ReadError}, edidPath));
            if (valid) {
                const unsigned manufacturer = (byte(8) << 8) | byte(9);
                QString manufacturerName;
                for (int shift : {10, 5, 0}) {
                    const unsigned character = (manufacturer >> shift) & 31;
                    if (character >= 1 && character <= 26) manufacturerName += QChar(static_cast<ushort>('A' + character - 1));
                }
                if (manufacturerName.size() != 3) manufacturerName.clear();
                fields.append(valueField(QStringLiteral("EDID manufacturer"), manufacturerName, edidPath));
                fields.append(valueField(QStringLiteral("EDID product code"), QStringLiteral("0x%1").arg(byte(10) | (byte(11) << 8), 4, 16, QLatin1Char('0')), edidPath));
                fields.append(valueField(QStringLiteral("EDID version"), QStringLiteral("%1.%2").arg(byte(18)).arg(byte(19)), edidPath));
                fields.append(valueField(QStringLiteral("Manufacture year"), QString::number(1990 + byte(17)), edidPath));
                if (byte(16) >= 1 && byte(16) <= 54)
                    fields.append(valueField(QStringLiteral("Manufacture week"), QString::number(byte(16)), edidPath));
                if (byte(21) && byte(22))
                    fields.append(valueField(QStringLiteral("Screen dimensions"), QStringLiteral("%1 × %2 cm").arg(byte(21)).arg(byte(22)), edidPath));
                const quint32 serial = quint32(byte(12)) | (quint32(byte(13)) << 8) | (quint32(byte(14)) << 16) | (quint32(byte(15)) << 24);
                if (serial) fields.append(valueField(QStringLiteral("EDID serial"), QString::number(serial), edidPath, true));
                for (qsizetype offset = 54; offset <= 108; offset += 18) {
                    if (byte(offset) || byte(offset + 1) || byte(offset + 2)) continue;
                    const QString text = QString::fromLatin1(bytes.mid(offset + 5, 13)).remove(QChar(0)).trimmed();
                    if (byte(offset + 3) == 0xfc) displayName = text;
                    if (byte(offset + 3) == 0xff) fields.append(valueField(QStringLiteral("EDID serial text"), text, edidPath, true));
                }
            }
#ifdef NEKO_HAS_DISPLAY_INFO
            fields.append(valueField(QStringLiteral("EDID decoder"), QStringLiteral("libdisplay-info"), QStringLiteral("libdisplay-info")));
            if (valid) {
                const std::unique_ptr<di_info, decltype(&di_info_destroy)> info(di_info_parse_edid(bytes.constData(), static_cast<size_t>(bytes.size())), &di_info_destroy);
                if (info) {
                    const auto stringValue = [](char *text) {
                        const QString value = QString::fromUtf8(text ? text : "");
                        std::free(text);
                        return value;
                    };
                    const QString model = stringValue(di_info_get_model(info.get()));
                    if (!model.isEmpty()) displayName = model;
                    fields.append(valueField(QStringLiteral("Display manufacturer"), stringValue(di_info_get_make(info.get())), edidPath + QStringLiteral(" via libdisplay-info")));
                    const QString serial = stringValue(di_info_get_serial(info.get()));
                    if (!serial.isEmpty()) fields.append(valueField(QStringLiteral("Display serial"), serial, edidPath + QStringLiteral(" via libdisplay-info"), true));
                    const char *diagnostics = di_info_get_failure_msg(info.get());
                    if (diagnostics) fields.append(field(QStringLiteral("EDID conformance diagnostics"), {QString::fromUtf8(diagnostics), ReadError}, edidPath + QStringLiteral(" via libdisplay-info")));
                } else {
                    fields.append(field(QStringLiteral("EDID conformance diagnostics"), {QStringLiteral("libdisplay-info could not parse EDID"), ReadError}, edidPath));
                }
            }
#else
            fields.append(valueField(QStringLiteral("EDID decoder"), QStringLiteral("Basic EDID base-block metadata"), edidPath));
#endif
            fields.append(valueField(QStringLiteral("EDID raw (hex)"), QString::fromLatin1(bytes.toHex(' ')),
                                     edidPath, true));
            fields.append(valueField(QStringLiteral("EDID bytes"), QString::number(bytes.size()), edidPath));
        } else {
            const QFileInfo info(edidPath);
            fields.append(field(QStringLiteral("EDID raw (hex)"),
                                {{}, info.exists() ? PermissionDenied : Unavailable}, edidPath, true));
        }
        const QString pci = QFileInfo(path + QStringLiteral("/device")).canonicalFilePath();
        snapshot.devices.append(device(QStringLiteral("display:") + entry, QStringLiteral("显示器"), displayName.isEmpty() ? entry : displayName,
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

void Collector::appendAudioCards(InventorySnapshot &snapshot)
{
    const QString root = pathFor(QStringLiteral("/proc/asound"));
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


} // namespace neko::inventory
