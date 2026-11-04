#include "InventoryContext.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace neko::inventory {

namespace {

QString ueventValue(const ReadResult &uevent, const QString &key)
{
    if (uevent.status != Available)
        return {};
    const QString prefix = key + QLatin1Char('=');
    for (const QString &line : uevent.value.split(QLatin1Char('\n'))) {
        if (line.startsWith(prefix))
            return line.mid(prefix.size()).trimmed();
    }
    return {};
}

bool isPciAddress(const QString &value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\\.[0-7]$"));
    return pattern.match(value).hasMatch();
}

} // namespace

void Collector::appendGpu(InventorySnapshot &snapshot)
{
    const QString root = pathFor(QStringLiteral("/sys/class/drm"));
    static const QRegularExpression cardPattern(QStringLiteral("^card[0-9]+$"));
    const QStringList cards = QDir(root).entryList({QStringLiteral("card[0-9]*")},
                                                   QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                   QDir::Name);
    for (const QString &card : cards) {
        if (!cardPattern.match(card).hasMatch())
            continue;
        const QString path = root + QLatin1Char('/') + card;
        const QString devicePath = path + QStringLiteral("/device");
        QVariantList fields{
            textField(QStringLiteral("Vendor ID"), devicePath + QStringLiteral("/vendor")),
            textField(QStringLiteral("Device ID"), devicePath + QStringLiteral("/device")),
            numericField(QStringLiteral("Boot VGA"), devicePath + QStringLiteral("/boot_vga"), QString()),
        };
        // PCI link metadata lives on the DRM card's PCI device, not on connectors.
        for (const QString &key : {QStringLiteral("current_link_speed"),
                                   QStringLiteral("current_link_width"),
                                   QStringLiteral("max_link_speed"),
                                   QStringLiteral("max_link_width")})
            fields.append(textField(key, devicePath + QLatin1Char('/') + key));
        const QString driverTarget = QFileInfo(devicePath + QStringLiteral("/driver")).symLinkTarget();
        const QString driver = driverTarget.isEmpty() ? QString() : QFileInfo(driverTarget).fileName();
        fields.append(valueField(QStringLiteral("Driver"), driver, devicePath + QStringLiteral("/driver")));

        const ReadResult uevent = readText(devicePath + QStringLiteral("/uevent"));
        QString pciAddress = ueventValue(uevent, QStringLiteral("PCI_SLOT_NAME"));
        if (pciAddress.isEmpty()) {
            const QString target = QFileInfo(devicePath).symLinkTarget();
            const QString targetName = QFileInfo(target).fileName();
            if (isPciAddress(targetName))
                pciAddress = targetName;
        }
        if (!pciAddress.isEmpty()) {
            fields.append(valueField(QStringLiteral("PCI address"), pciAddress,
                                     devicePath + QStringLiteral("/uevent")));
        } else {
            QVariantMap address = field(QStringLiteral("PCI address"),
                                        {{}, uevent.status == Available ? ReadError : uevent.status},
                                        devicePath + QStringLiteral("/uevent"));
            address.insert(QStringLiteral("reason"),
                           uevent.status == Available
                               ? QStringLiteral("kernel uevent does not expose PCI_SLOT_NAME")
                               : QStringLiteral("attribute is absent or empty"));
            fields.append(address);
        }

        // amdgpu exposes memory and utilization counters as direct device attributes.
        fields.append(bytesField(QStringLiteral("amdgpu VRAM total"),
                                 devicePath + QStringLiteral("/mem_info_vram_total")));
        fields.append(bytesField(QStringLiteral("amdgpu VRAM used"),
                                 devicePath + QStringLiteral("/mem_info_vram_used")));
        fields.append(numericField(QStringLiteral("amdgpu GPU busy"),
                                   devicePath + QStringLiteral("/gpu_busy_percent"), QStringLiteral("%")));
        fields.append(numericField(QStringLiteral("amdgpu memory busy"),
                                   devicePath + QStringLiteral("/mem_busy_percent"), QStringLiteral("%")));

        // i915 exposes frequency counters in MHz.
        fields.append(numericField(QStringLiteral("i915 current frequency"),
                                   devicePath + QStringLiteral("/gt_cur_freq_mhz"), QStringLiteral("MHz")));
        fields.append(numericField(QStringLiteral("i915 max frequency"),
                                   devicePath + QStringLiteral("/gt_max_freq_mhz"), QStringLiteral("MHz")));
        fields.append(numericField(QStringLiteral("i915 RP0 frequency"),
                                   devicePath + QStringLiteral("/gt_RP0_freq_mhz"), QStringLiteral("MHz")));

        // hwmon may be absent on drivers that do not expose sensors.
        const QString hwmonRoot = devicePath + QStringLiteral("/hwmon");
        const QStringList hwmons = QDir(hwmonRoot).entryList({QStringLiteral("hwmon*")},
                                                             QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                             QDir::Name);
        if (hwmons.isEmpty()) {
            for (const QString &key : {QStringLiteral("Temperature"), QStringLiteral("Power"),
                                       QStringLiteral("Fan")}) {
                QVariantMap item = field(key, {{}, Unavailable}, hwmonRoot);
                item.insert(QStringLiteral("reason"),
                            QStringLiteral("GPU hwmon interface is not present"));
                fields.append(item);
            }
        } else {
            int index = 0;
            for (const QString &hwmon : hwmons) {
                const QString base = hwmonRoot + QLatin1Char('/') + hwmon;
                const QString suffix = index++ == 0 ? QString()
                                                    : QStringLiteral(" %1").arg(index);
                fields.append(numericField(QStringLiteral("Temperature") + suffix,
                                           base + QStringLiteral("/temp1_input"),
                                           QStringLiteral("°C"), 1000.0, 1));
                QString powerPath = base + QStringLiteral("/power1_average");
                if (!QFileInfo(powerPath).exists())
                    powerPath = base + QStringLiteral("/power1_input");
                fields.append(numericField(QStringLiteral("Power") + suffix, powerPath,
                                           QStringLiteral("W"), 1000000.0, 2));
                fields.append(numericField(QStringLiteral("Fan") + suffix,
                                           base + QStringLiteral("/fan1_input"),
                                           QStringLiteral("RPM"), 1.0, 0));
            }
        }

        const QString name = (driver + QLatin1Char(' ') + pciAddress).trimmed();
        snapshot.devices.append(device(QStringLiteral("gpu:") + card, QStringLiteral("显卡"),
                                       name.isEmpty() ? card : name,
                                       QStringLiteral("DRM/KMS card %1").arg(card),
                                       path, fields));
    }
}

} // namespace neko::inventory
