#include "InventoryCollector.h"
#include "inventory/InventorySource.h"
#include "inventory/InventoryContext.h"

#include <QSysInfo>

namespace neko::inventory {

InventorySnapshot Collector::run()
{
    InventorySnapshot snapshot;
    snapshot.system.insert(QStringLiteral("os"), osPrettyName());
    snapshot.system.insert(QStringLiteral("kernel"), source.root.isEmpty() ? QSysInfo::kernelVersion() : readText(pathFor(QStringLiteral("/proc/sys/kernel/osrelease"))).value);
    snapshot.system.insert(QStringLiteral("architecture"), source.root.isEmpty() ? QSysInfo::currentCpuArchitecture() : QString());
    snapshot.system.insert(QStringLiteral("hostname"), readText(pathFor(QStringLiteral("/proc/sys/kernel/hostname"))).value);
    snapshot.system.insert(QStringLiteral("uptime"), uptimeText());
    snapshot.system.insert(QStringLiteral("collectedAt"), source.collectedAt.toUTC().toString(Qt::ISODateWithMs));
    snapshot.system.insert(QStringLiteral("collectorVersion"), QStringLiteral("linux-sysfs-2"));
    snapshot.system.insert(QStringLiteral("provenance"), QVariantMap{
        {QStringLiteral("os"), pathFor(QStringLiteral("/etc/os-release"))},
        {QStringLiteral("kernel"), pathFor(QStringLiteral("/proc/sys/kernel/osrelease"))},
        {QStringLiteral("hostname"), pathFor(QStringLiteral("/proc/sys/kernel/hostname"))},
        {QStringLiteral("architecture"), QStringLiteral("QSysInfo::currentCpuArchitecture")},
        {QStringLiteral("uptime"), pathFor(QStringLiteral("/proc/uptime"))},
        {QStringLiteral("cpu"), pathFor(QStringLiteral("/proc/cpuinfo"))},
        {QStringLiteral("logicalCpus"), pathFor(QStringLiteral("/sys/devices/system/cpu"))},
        {QStringLiteral("memoryTotal"), pathFor(QStringLiteral("/proc/meminfo"))}});
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
    for (QVariant &value : snapshot.devices) {
        auto item = value.toMap();
        item.insert(QStringLiteral("collectedAt"), snapshot.system.value(QStringLiteral("collectedAt")));
        item.insert(QStringLiteral("collectorVersion"), snapshot.system.value(QStringLiteral("collectorVersion")));
        auto fields = item.value(QStringLiteral("fields")).toList();
        for (QVariant &field : fields) {
            auto map = field.toMap();
            map.insert(QStringLiteral("collectedAt"), snapshot.system.value(QStringLiteral("collectedAt")));
            field = map;
        }
        item.insert(QStringLiteral("fields"), fields);
        value = item;
    }
    return snapshot;
}

InventorySnapshot collectInventory(const InventorySource &source)
{
    return Collector(source).run();
}

} // namespace neko::inventory

namespace neko {
InventorySnapshot collectInventory()
{
    return inventory::collectInventory(inventory::InventorySource{});
}
} // namespace neko
