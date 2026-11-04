#include "../InventoryCollector.h"
#include "../inventory/InventorySource.h"

#include <QTest>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

namespace {
void writeFile(const QTemporaryDir &root, const QString &path, const QByteArray &data)
{
    const QString full = root.path() + path;
    QVERIFY(QDir().mkpath(QFileInfo(full).absolutePath()));
    QFile file(full);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), data.size());
}
neko::InventorySnapshot fixture(const QTemporaryDir &root)
{
    return neko::inventory::collectInventory({root.path(), QDateTime::fromString(QStringLiteral("2026-11-04T08:00:00.000Z"), Qt::ISODateWithMs)});
}
QVariantMap deviceById(const neko::InventorySnapshot &snapshot, const QString &id)
{
    for (const auto &device : snapshot.devices)
        if (device.toMap().value(QStringLiteral("id")).toString() == id)
            return device.toMap();
    return {};
}
QVariantMap fieldByKey(const QVariantMap &device, const QString &key)
{
    for (const auto &field : device.value(QStringLiteral("fields")).toList())
        if (field.toMap().value(QStringLiteral("key")).toString() == key)
            return field.toMap();
    return {};
}
QByteArray validEdid()
{
    QByteArray bytes(128, '\0');
    const QByteArray header = QByteArray::fromHex("00ffffffffffff00");
    bytes.replace(0, 8, header);
    bytes[8] = 0x10; bytes[9] = char(0xac); // DEL
    bytes[10] = 0x34; bytes[11] = 0x12;
    bytes[16] = 20; bytes[17] = 30; // 2020
    bytes[18] = 1; bytes[19] = 4;
    bytes[21] = 52; bytes[22] = 29;
    bytes[57] = char(0xfc);
    bytes.replace(59, 13, QByteArray("Fixture Panel"));
    unsigned sum = 0;
    for (int i = 0; i < 127; ++i) sum += static_cast<unsigned char>(bytes[i]);
    bytes[127] = static_cast<char>((256 - sum % 256) % 256);
    return bytes;
}
} // namespace

class InventoryTest final : public QObject {
    Q_OBJECT

private slots:
    void collectsRealSystemMetadata()
    {
        const neko::InventorySnapshot snapshot = neko::collectInventory();
        QVERIFY(!snapshot.system.isEmpty());
        QVERIFY(!snapshot.system.value(QStringLiteral("os")).toString().isEmpty());
        QVERIFY(!snapshot.system.value(QStringLiteral("kernel")).toString().isEmpty());
        QVERIFY(!snapshot.system.value(QStringLiteral("architecture")).toString().isEmpty());
        QVERIFY(!snapshot.system.value(QStringLiteral("hostname")).toString().isEmpty());
        QVERIFY(!snapshot.system.value(QStringLiteral("logicalCpus")).toString().isEmpty());
        QVERIFY(snapshot.system.contains(QStringLiteral("memoryTotal")));
        QVERIFY(!snapshot.devices.isEmpty());
    }

    void inventoryFieldsHaveProvenanceAndStatus()
    {
        const neko::InventorySnapshot snapshot = neko::collectInventory();
        bool foundSource = false;
        for (const QVariant &item : snapshot.devices) {
            const QVariantMap device = item.toMap();
            QVERIFY(!device.value(QStringLiteral("id")).toString().isEmpty());
            QVERIFY(!device.value(QStringLiteral("category")).toString().isEmpty());
            QVERIFY(!device.value(QStringLiteral("source")).toString().isEmpty());
            QVERIFY(device.value(QStringLiteral("status")).toString() == QStringLiteral("available") ||
                    device.value(QStringLiteral("status")).toString() == QStringLiteral("permission-denied") ||
                    device.value(QStringLiteral("status")).toString() == QStringLiteral("unavailable"));
            const QVariantList fields = device.value(QStringLiteral("fields")).toList();
            QVERIFY(!fields.isEmpty());
            for (const QVariant &fieldValue : fields) {
                const QVariantMap field = fieldValue.toMap();
                QVERIFY(field.contains(QStringLiteral("key")));
                QVERIFY(field.contains(QStringLiteral("value")));
                QVERIFY(!field.value(QStringLiteral("source")).toString().isEmpty());
                QVERIFY(field.contains(QStringLiteral("sensitive")));
                const QString status = field.value(QStringLiteral("status")).toString();
                QVERIFY(status == QStringLiteral("available") || status == QStringLiteral("permission-denied") ||
                        status == QStringLiteral("unavailable") || status == QStringLiteral("read-error"));
                foundSource = foundSource || field.value(QStringLiteral("source")).toString().startsWith('/');
            }
        }
        QVERIFY(foundSource);
    }

    void emptyRootDoesNotInventMemoryOrTopology()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto snapshot = fixture(root);
        QCOMPARE(snapshot.system.value(QStringLiteral("hostname")).toString(), QString());
        QCOMPARE(snapshot.system.value(QStringLiteral("os")).toString(), QString());
        const auto cpu = deviceById(snapshot, QStringLiteral("cpu:system"));
        QCOMPARE(fieldByKey(cpu, QStringLiteral("Cores")).value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QCOMPARE(cpu.value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QCOMPARE(fieldByKey(cpu, QStringLiteral("Logical CPUs")).value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        const auto memory = deviceById(snapshot, QStringLiteral("memory:system"));
        QCOMPARE(memory.value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QCOMPARE(fieldByKey(memory, QStringLiteral("MemTotal")).value(QStringLiteral("value")).toString(), QString());
        QCOMPARE(fixture(root).devices, snapshot.devices);
    }

    void memoryCountsAndBytesRemainDistinct()
    {
        QTemporaryDir root;
        writeFile(root, QStringLiteral("/proc/meminfo"), "MemTotal: 8192 kB\nMemAvailable: 4096 kB\nHugePages_Total: 7\nHugePages_Free: 0\nHugepagesize: 2048 kB\n");
        const auto snapshot = fixture(root);
        const auto memory = deviceById(snapshot, QStringLiteral("memory:system"));
        QCOMPARE(fieldByKey(memory, QStringLiteral("MemTotal")).value(QStringLiteral("value")).toString(), QStringLiteral("8.00 MiB"));
        const auto count = fieldByKey(memory, QStringLiteral("HugePages_Total"));
        QCOMPARE(count.value(QStringLiteral("value")).toString(), QStringLiteral("7"));
        QCOMPARE(count.value(QStringLiteral("unit")).toString(), QStringLiteral("count"));
        QCOMPARE(fieldByKey(memory, QStringLiteral("HugePages_Free")).value(QStringLiteral("status")).toString(), QStringLiteral("available"));
        QCOMPARE(fieldByKey(memory, QStringLiteral("Used memory")).value(QStringLiteral("value")).toString(), QStringLiteral("4.00 MiB"));
    }

    void malformedMemoryAndBlockCountersAreNotZero()
    {
        QTemporaryDir root;
        writeFile(root, QStringLiteral("/proc/meminfo"), "MemTotal: 18446744073709551615 kB\nMemAvailable: 12 garbage\nHugePages_Total: -1\n");
        writeFile(root, QStringLiteral("/sys/class/block/sda/size"), "not-a-sector-count\n");
        const auto snapshot = fixture(root);
        const auto memory = deviceById(snapshot, QStringLiteral("memory:system"));
        QCOMPARE(fieldByKey(memory, QStringLiteral("MemTotal")).value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
        QCOMPARE(fieldByKey(memory, QStringLiteral("MemAvailable")).value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
        const auto disk = deviceById(snapshot, QStringLiteral("storage:sda"));
        QCOMPARE(fieldByKey(disk, QStringLiteral("Capacity")).value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
    }

    void missingTopologyDoesNotInventOneCore()
    {
        QTemporaryDir root;
        writeFile(root, QStringLiteral("/proc/cpuinfo"), "processor : 0\nmodel name : AMD Ryzen 9 7950X\n");
        QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/sys/devices/system/cpu/cpu0")));
        const auto snapshot = fixture(root);
        QCOMPARE(snapshot.system.value(QStringLiteral("cpu")).toString(), QStringLiteral("AMD Ryzen 9 7950X"));
        const auto cpu = deviceById(snapshot, QStringLiteral("cpu:system"));
        QCOMPARE(fieldByKey(cpu, QStringLiteral("Cores")).value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QCOMPARE(fieldByKey(cpu, QStringLiteral("Packages")).value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
    }

    void cacheAndNumaDataHaveExactProvenance()
    {
        QTemporaryDir root;
        for (const QString &cpu : {QStringLiteral("cpu0"), QStringLiteral("cpu1")}) {
            const QString base = QStringLiteral("/sys/devices/system/cpu/") + cpu;
            writeFile(root, base + QStringLiteral("/topology/physical_package_id"), "0\n");
            writeFile(root, base + QStringLiteral("/topology/core_id"), "0\n");
            writeFile(root, base + QStringLiteral("/topology/thread_siblings_list"), "0-1\n");
            writeFile(root, base + QStringLiteral("/cache/index0/level"), "3\n");
            writeFile(root, base + QStringLiteral("/cache/index0/type"), "Unified\n");
            writeFile(root, base + QStringLiteral("/cache/index0/size"), "32768K\n");
            writeFile(root, base + QStringLiteral("/cache/index0/shared_cpu_list"), "0-1\n");
        }
        writeFile(root, QStringLiteral("/sys/devices/system/node/node0/cpulist"), "0-1\n");
        writeFile(root, QStringLiteral("/sys/devices/system/node/node0/distance"), "10 21\n");
        const auto snapshot = fixture(root);
        const auto cpu = deviceById(snapshot, QStringLiteral("cpu:system"));
        QCOMPARE(fieldByKey(cpu, QStringLiteral("Packages")).value(QStringLiteral("value")).toString(), QStringLiteral("1"));
        QCOMPARE(fieldByKey(cpu, QStringLiteral("Cores")).value(QStringLiteral("value")).toString(), QStringLiteral("1"));
        const auto cache = fieldByKey(cpu, QStringLiteral("Cache 1 size"));
        QCOMPARE(cache.value(QStringLiteral("value")).toString(), QStringLiteral("32768K"));
        QCOMPARE(cache.value(QStringLiteral("source")).toString(), root.path() + QStringLiteral("/sys/devices/system/cpu/cpu0/cache/index0/size"));
        QVERIFY(fieldByKey(cpu, QStringLiteral("Cache 2 size")).isEmpty());
        const auto memory = deviceById(snapshot, QStringLiteral("memory:system"));
        QCOMPARE(fieldByKey(memory, QStringLiteral("node0 distance")).value(QStringLiteral("value")).toString(), QStringLiteral("10 21"));
        QCOMPARE(fixture(root).devices, snapshot.devices);
    }

    void malformedMeminfoUnitsAreNotAcceptedAsCapacity()
    {
        QTemporaryDir root;
        writeFile(root, QStringLiteral("/proc/meminfo"), "MemTotal: 1024\nHugePages_Total: 7 kB\n");
        const auto memory = deviceById(fixture(root), QStringLiteral("memory:system"));
        QCOMPARE(fieldByKey(memory, QStringLiteral("MemTotal")).value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
        QCOMPARE(fieldByKey(memory, QStringLiteral("HugePages_Total")).value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
    }

    void mountInfoAssociatesFilesystemWithBlockDevice()
    {
        QTemporaryDir root;
        writeFile(root, QStringLiteral("/sys/class/block/sda/dev"), "8:0\n");
        writeFile(root, QStringLiteral("/sys/class/block/sda/size"), "2048\n");
        writeFile(root, QStringLiteral("/proc/self/mountinfo"), "24 23 8:0 / /media/My\\040Disk rw - ext4 /dev/sda rw\n25 23 0:2 / /sys rw - sysfs sysfs rw\n");
        const auto disk = deviceById(fixture(root), QStringLiteral("storage:sda"));
        QCOMPARE(fieldByKey(disk, QStringLiteral("Mount point")).value(QStringLiteral("value")).toString(), QStringLiteral("/media/My Disk"));
        QCOMPARE(fieldByKey(disk, QStringLiteral("Filesystem")).value(QStringLiteral("value")).toString(), QStringLiteral("ext4"));
        QCOMPARE(fieldByKey(disk, QStringLiteral("Capacity")).value(QStringLiteral("value")).toString(), QStringLiteral("1.00 MiB"));
        QVERIFY(fieldByKey(disk, QStringLiteral("Mount point")).value(QStringLiteral("sensitive")).toBool());
        QCOMPARE(fieldByKey(disk, QStringLiteral("SMART / health")).value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
    }

    void dmiNameUsesTheConfiguredRoot()
    {
        QTemporaryDir root;
        writeFile(root, QStringLiteral("/sys/class/dmi/id/sys_vendor"), "Fixture Vendor\n");
        writeFile(root, QStringLiteral("/sys/class/dmi/id/product_name"), "Fixture Board\n");
        writeFile(root, QStringLiteral("/sys/class/dmi/id/product_serial"), "PRIVATE-SERIAL\n");
        const auto board = deviceById(fixture(root), QStringLiteral("system:board"));
        QCOMPARE(board.value(QStringLiteral("name")).toString(), QStringLiteral("Fixture Vendor Fixture Board"));
        QVERIFY(fieldByKey(board, QStringLiteral("product_serial")).value(QStringLiteral("sensitive")).toBool());
    }

    void malformedUptimeDoesNotOverflow()
    {
        QTemporaryDir root;
        for (const QByteArray &value : {QByteArray("-1.5 1"), QByteArray("nan 1"), QByteArray("inf 1"), QByteArray("1e100 1")}) {
            writeFile(root, QStringLiteral("/proc/uptime"), value);
            QVERIFY2(fixture(root).system.value(QStringLiteral("uptime")).toString().isEmpty(), value.constData());
        }
        writeFile(root, QStringLiteral("/proc/uptime"), "90061.5 1");
        QCOMPARE(fixture(root).system.value(QStringLiteral("uptime")).toString(), QStringLiteral("1 d 1 h 1 min"));
    }

    void pciNamesComeFromSystemDatabase()
    {
        QTemporaryDir root;
        writeFile(root, QStringLiteral("/sys/bus/pci/devices/0000:01:00.0/vendor"), "0x10de\n");
        writeFile(root, QStringLiteral("/sys/bus/pci/devices/0000:01:00.0/device"), "0x2704\n");
        writeFile(root, QStringLiteral("/usr/share/hwdata/pci.ids"), "10de  NVIDIA Corporation\n\t2704  GeForce RTX 4080\n");
        const auto gpu = deviceById(fixture(root), QStringLiteral("pci:0000:01:00.0"));
        QCOMPARE(gpu.value(QStringLiteral("name")).toString(), QStringLiteral("NVIDIA Corporation GeForce RTX 4080"));
    }

    void edidProvidesValidatedDisplayDetails()
    {
        QTemporaryDir root;
        writeFile(root, QStringLiteral("/sys/class/drm/card0-DP-1/status"), "connected\n");
        writeFile(root, QStringLiteral("/sys/class/drm/card0-DP-1/edid"), validEdid());
        const auto display = deviceById(fixture(root), QStringLiteral("display:card0-DP-1"));
        QCOMPARE(display.value(QStringLiteral("name")).toString(), QStringLiteral("Fixture Panel"));
        QVERIFY(!fieldByKey(display, QStringLiteral("EDID decoder")).value(QStringLiteral("value")).toString().isEmpty());
        QCOMPARE(fieldByKey(display, QStringLiteral("EDID manufacturer")).value(QStringLiteral("value")).toString(), QStringLiteral("DEL"));
        QCOMPARE(fieldByKey(display, QStringLiteral("Manufacture year")).value(QStringLiteral("value")).toString(), QStringLiteral("2020"));
        QCOMPARE(fieldByKey(display, QStringLiteral("EDID validation")).value(QStringLiteral("status")).toString(), QStringLiteral("available"));
    }

    void malformedEdidIsReportedWithoutDecodedIdentity()
    {
        QTemporaryDir root;
        auto bytes = validEdid(); bytes[20] = 42;
        writeFile(root, QStringLiteral("/sys/class/drm/card0-DP-1/edid"), bytes);
        const auto display = deviceById(fixture(root), QStringLiteral("display:card0-DP-1"));
        QCOMPARE(fieldByKey(display, QStringLiteral("EDID validation")).value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
        QVERIFY(fieldByKey(display, QStringLiteral("EDID manufacturer")).isEmpty());
    }

    void cpuModelComesFromKernel()
    {
        QFile cpuinfo(QStringLiteral("/proc/cpuinfo"));
        QVERIFY(cpuinfo.open(QIODevice::ReadOnly));
        const QString contents = QString::fromUtf8(cpuinfo.readAll());
        const auto snapshot = neko::collectInventory();
        const QString cpu = snapshot.system.value(QStringLiteral("cpu")).toString();
        if (cpu != QStringLiteral("Unknown"))
            QVERIFY2(contents.contains(cpu), qPrintable(cpu));
    }

    void timestampsAreAttachedToEveryField()
    {
        const auto snapshot = neko::collectInventory();
        const QString timestamp = snapshot.system.value(QStringLiteral("collectedAt")).toString();
        QVERIFY(QDateTime::fromString(timestamp, Qt::ISODateWithMs).isValid());
        for (const QVariant &item : snapshot.devices) {
            const auto device = item.toMap();
            QCOMPARE(device.value(QStringLiteral("collectedAt")).toString(), timestamp);
            for (const QVariant &value : device.value(QStringLiteral("fields")).toList())
                QCOMPARE(value.toMap().value(QStringLiteral("collectedAt")).toString(), timestamp);
        }
    }
};

QTEST_GUILESS_MAIN(InventoryTest)
#include "tst_inventory.moc"
