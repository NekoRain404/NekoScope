#include "InventoryContext.h"

#include <QDir>
#include <QRegularExpression>

#include <limits>

namespace neko::inventory {

void Collector::appendNvme(InventorySnapshot &snapshot)
{
    const QString root = pathFor(QStringLiteral("/sys/class/nvme"));
    static const QRegularExpression controllerPattern(QStringLiteral("^nvme[0-9]+$"));
    const QStringList controllers = QDir(root).entryList({QStringLiteral("nvme[0-9]*")},
                                                         QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                         QDir::Name);
    for (const QString &controller : controllers) {
        if (!controllerPattern.match(controller).hasMatch())
            continue;
        const QString path = root + QLatin1Char('/') + controller;
        QVariantList fields{
            textField(QStringLiteral("Model"), path + QStringLiteral("/model")),
            textField(QStringLiteral("Firmware"), path + QStringLiteral("/firmware_rev")),
            textField(QStringLiteral("Serial"), path + QStringLiteral("/serial"), true),
            textField(QStringLiteral("Transport"), path + QStringLiteral("/transport")),
            textField(QStringLiteral("Address"), path + QStringLiteral("/address")),
            textField(QStringLiteral("Controller state"), path + QStringLiteral("/state")),
            textField(QStringLiteral("Controller type"), path + QStringLiteral("/cntrltype")),
            textField(QStringLiteral("Queue count"), path + QStringLiteral("/queue_count")),
        };

        const QString namespaceFilter = controller + QStringLiteral("n[0-9]*");
        for (const QString &nameSpace : QDir(path).entryList({namespaceFilter},
                                                             QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                             QDir::Name)) {
            const QString nsPath = path + QLatin1Char('/') + nameSpace;
            const QString suffix = nameSpace.mid(controller.size() + 1);
            const QString label = QStringLiteral("Namespace ") +
                                  (suffix.isEmpty() ? nameSpace : suffix);
            const QString sizePath = nsPath + QStringLiteral("/size");
            QVariantMap sectors = numericField(label + QStringLiteral(" sectors"), sizePath,
                                               QStringLiteral("sectors"));
            const QString sectorsStatus = sectors.value(QStringLiteral("status")).toString();
            fields.append(sectors);
            if (sectorsStatus == Available) {
                const quint64 count = sectors.value(QStringLiteral("rawValue")).toULongLong();
                if (count <= std::numeric_limits<quint64>::max() / 512ULL) {
                    QVariantMap size = field(label + QStringLiteral(" size"),
                                             {prettyBytes(count * 512ULL), Available}, sizePath);
                    size.insert(QStringLiteral("rawValue"),
                                QVariant::fromValue<quint64>(count * 512ULL));
                    size.insert(QStringLiteral("unit"), QStringLiteral("bytes"));
                    fields.append(size);
                } else {
                    QVariantMap size = field(label + QStringLiteral(" size"),
                                             {{}, ReadError}, sizePath);
                    size.insert(QStringLiteral("reason"),
                                QStringLiteral("sector count overflows byte capacity"));
                    fields.append(size);
                }
            } else {
                QVariantMap size = field(label + QStringLiteral(" size"),
                                         {{}, sectorsStatus}, sizePath);
                if (sectors.contains(QStringLiteral("reason")))
                    size.insert(QStringLiteral("reason"), sectors.value(QStringLiteral("reason")));
                fields.append(size);
            }
            fields.append(numericField(label + QStringLiteral(" block size"),
                                       nsPath + QStringLiteral("/queue/logical_block_size"),
                                       QStringLiteral("bytes")));
            fields.append(textField(label + QStringLiteral(" WWID"), nsPath + QStringLiteral("/wwid"),
                                    true));
        }

        // The nvme driver publishes its hwmon device beside the controller attributes.
        const QStringList hwmons = QDir(path).entryList({QStringLiteral("hwmon*")},
                                                         QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                         QDir::Name);
        if (hwmons.isEmpty()) {
            QVariantMap temperature = field(QStringLiteral("Temperature"),
                                            {{}, Unavailable}, path + QStringLiteral("/hwmon"));
            temperature.insert(QStringLiteral("reason"),
                               QStringLiteral("NVMe hwmon interface is not present"));
            fields.append(temperature);
        } else {
            int index = 0;
            for (const QString &hwmon : hwmons) {
                const QString base = path + QLatin1Char('/') + hwmon;
                const QString key = index++ == 0 ? QStringLiteral("Temperature")
                                                 : QStringLiteral("Temperature %1").arg(index);
                fields.append(numericField(key, base + QStringLiteral("/temp1_input"),
                                           QStringLiteral("°C"), 1000.0, 1));
            }
        }

        QVariantMap smart = field(QStringLiteral("SMART / health"), {{}, Unavailable},
                                  QStringLiteral("kernel NVMe sysfs (no SMART attribute exposed)"));
        smart.insert(QStringLiteral("reason"),
                     QStringLiteral("No unprivileged kernel interface exposes NVMe SMART health; "
                                    "privileged ioctls are not used"));
        fields.append(smart);

        const QString model = readText(path + QStringLiteral("/model")).value;
        const QString transport = readText(path + QStringLiteral("/transport")).value;
        const QString address = readText(path + QStringLiteral("/address")).value;
        snapshot.devices.append(device(QStringLiteral("nvme:") + controller, QStringLiteral("存储"),
                                       model.isEmpty() ? controller : model,
                                       (transport + QLatin1Char(' ') + address).trimmed(),
                                       path, fields));
    }
}

} // namespace neko::inventory
