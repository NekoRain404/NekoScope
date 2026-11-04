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

    void gpuDetailReadsAmdgpuAndHwmon()
    {
        QTemporaryDir root;
        const QString gpu = QStringLiteral("/sys/class/drm/card0/device");
        writeFile(root, gpu + QStringLiteral("/vendor"), "0x1002\n");
        writeFile(root, gpu + QStringLiteral("/device"), "0x73bf\n");
        writeFile(root, gpu + QStringLiteral("/boot_vga"), "1\n");
        writeFile(root, gpu + QStringLiteral("/current_link_speed"), "16.0 GT/s\n");
        writeFile(root, gpu + QStringLiteral("/current_link_width"), "8\n");
        writeFile(root, gpu + QStringLiteral("/max_link_speed"), "32.0 GT/s\n");
        writeFile(root, gpu + QStringLiteral("/max_link_width"), "16\n");
        writeFile(root, gpu + QStringLiteral("/uevent"),
                  "DRIVER=amdgpu\nPCI_SLOT_NAME=0000:03:00.0\n");
        writeFile(root, gpu + QStringLiteral("/mem_info_vram_total"), "17179869184\n");
        writeFile(root, gpu + QStringLiteral("/mem_info_vram_used"), "2147483648\n");
        writeFile(root, gpu + QStringLiteral("/gpu_busy_percent"), "42\n");
        writeFile(root, gpu + QStringLiteral("/mem_busy_percent"), "7\n");
        writeFile(root, gpu + QStringLiteral("/hwmon/hwmon0/temp1_input"), "45000\n");
        writeFile(root, gpu + QStringLiteral("/hwmon/hwmon0/power1_average"), "125000000\n");
        writeFile(root, gpu + QStringLiteral("/hwmon/hwmon0/fan1_input"), "1200\n");
        QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/sys/class/drm/card0-DP-1")));

        const auto snapshot = fixture(root);
        const auto gpuDevice = deviceById(snapshot, QStringLiteral("gpu:card0"));
        QCOMPARE(gpuDevice.value(QStringLiteral("category")).toString(), QStringLiteral("显卡"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("Vendor ID")).value(QStringLiteral("value")).toString(), QStringLiteral("0x1002"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("PCI address")).value(QStringLiteral("value")).toString(), QStringLiteral("0000:03:00.0"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("Boot VGA")).value(QStringLiteral("value")).toString(), QStringLiteral("1"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("current_link_speed")).value(QStringLiteral("value")).toString(), QStringLiteral("16.0 GT/s"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("current_link_width")).value(QStringLiteral("value")).toString(), QStringLiteral("8"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("max_link_speed")).value(QStringLiteral("value")).toString(), QStringLiteral("32.0 GT/s"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("max_link_width")).value(QStringLiteral("value")).toString(), QStringLiteral("16"));
        const auto vram = fieldByKey(gpuDevice, QStringLiteral("amdgpu VRAM total"));
        QCOMPARE(vram.value(QStringLiteral("value")).toString(), QStringLiteral("16.00 GiB"));
        QCOMPARE(vram.value(QStringLiteral("unit")).toString(), QStringLiteral("bytes"));
        QCOMPARE(vram.value(QStringLiteral("rawValue")).toULongLong(), Q_UINT64_C(17179869184));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("amdgpu VRAM used")).value(QStringLiteral("value")).toString(), QStringLiteral("2.00 GiB"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("amdgpu GPU busy")).value(QStringLiteral("value")).toString(), QStringLiteral("42"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("amdgpu memory busy")).value(QStringLiteral("value")).toString(), QStringLiteral("7"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("Temperature")).value(QStringLiteral("value")).toString(), QStringLiteral("45.0"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("Temperature")).value(QStringLiteral("unit")).toString(), QStringLiteral("°C"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("Power")).value(QStringLiteral("value")).toString(), QStringLiteral("125.00"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("Power")).value(QStringLiteral("unit")).toString(), QStringLiteral("W"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("Fan")).value(QStringLiteral("value")).toString(), QStringLiteral("1200"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("i915 current frequency")).value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QVERIFY(!fieldByKey(gpuDevice, QStringLiteral("i915 current frequency")).value(QStringLiteral("reason")).toString().isEmpty());
        QVERIFY(deviceById(snapshot, QStringLiteral("gpu:card0-DP-1")).isEmpty());
    }

    void gpuMalformedAndMissingValuesAreNotZero()
    {
        QTemporaryDir root;
        const QString gpu = QStringLiteral("/sys/class/drm/card0/device");
        writeFile(root, gpu + QStringLiteral("/mem_info_vram_total"), "garbage\n");
        writeFile(root, gpu + QStringLiteral("/gpu_busy_percent"), "\n");
        const auto gpuDevice = deviceById(fixture(root), QStringLiteral("gpu:card0"));
        const auto malformed = fieldByKey(gpuDevice, QStringLiteral("amdgpu VRAM total"));
        QCOMPARE(malformed.value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
        QCOMPARE(malformed.value(QStringLiteral("value")).toString(), QString());
        QVERIFY(!malformed.value(QStringLiteral("reason")).toString().isEmpty());
        const auto absent = fieldByKey(gpuDevice, QStringLiteral("amdgpu GPU busy"));
        QCOMPARE(absent.value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QCOMPARE(absent.value(QStringLiteral("value")).toString(), QString());
        QVERIFY(!absent.value(QStringLiteral("reason")).toString().isEmpty());
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("amdgpu VRAM used")).value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QCOMPARE(fieldByKey(gpuDevice, QStringLiteral("Temperature")).value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QVERIFY(!fieldByKey(gpuDevice, QStringLiteral("Temperature")).value(QStringLiteral("reason")).toString().isEmpty());
    }

    void nvmeIdentityNamespaceAndTemperature()
    {
        QTemporaryDir root;
        const QString base = QStringLiteral("/sys/class/nvme/nvme0");
        writeFile(root, base + QStringLiteral("/model"), "Fixture NVMe\n");
        writeFile(root, base + QStringLiteral("/firmware_rev"), "FW1.2\n");
        writeFile(root, base + QStringLiteral("/serial"), "SN-PRIVATE\n");
        writeFile(root, base + QStringLiteral("/transport"), "pcie\n");
        writeFile(root, base + QStringLiteral("/address"), "0000:55:00.0\n");
        writeFile(root, base + QStringLiteral("/state"), "live\n");
        writeFile(root, base + QStringLiteral("/cntrltype"), "io\n");
        writeFile(root, base + QStringLiteral("/queue_count"), "8\n");
        writeFile(root, base + QStringLiteral("/hwmon0/temp1_input"), "38000\n");
        writeFile(root, base + QStringLiteral("/nvme0n1/size"), "2000409\n");
        writeFile(root, base + QStringLiteral("/nvme0n1/queue/logical_block_size"), "512\n");
        writeFile(root, base + QStringLiteral("/nvme0n1/wwid"), "eui.1234\n");
        const auto nvme = deviceById(fixture(root), QStringLiteral("nvme:nvme0"));
        QCOMPARE(nvme.value(QStringLiteral("category")).toString(), QStringLiteral("存储"));
        QCOMPARE(nvme.value(QStringLiteral("name")).toString(), QStringLiteral("Fixture NVMe"));
        QCOMPARE(fieldByKey(nvme, QStringLiteral("Controller state")).value(QStringLiteral("value")).toString(), QStringLiteral("live"));
        const auto serial = fieldByKey(nvme, QStringLiteral("Serial"));
        QVERIFY(serial.value(QStringLiteral("sensitive")).toBool());
        QCOMPARE(serial.value(QStringLiteral("value")).toString(), QStringLiteral("SN-PRIVATE"));
        QCOMPARE(fieldByKey(nvme, QStringLiteral("Namespace 1 sectors")).value(QStringLiteral("value")).toString(), QStringLiteral("2000409"));
        QCOMPARE(fieldByKey(nvme, QStringLiteral("Namespace 1 sectors")).value(QStringLiteral("unit")).toString(), QStringLiteral("sectors"));
        const auto size = fieldByKey(nvme, QStringLiteral("Namespace 1 size"));
        QCOMPARE(size.value(QStringLiteral("value")).toString(), QStringLiteral("976.76 MiB"));
        QCOMPARE(size.value(QStringLiteral("rawValue")).toULongLong(), Q_UINT64_C(1024209408));
        QCOMPARE(fieldByKey(nvme, QStringLiteral("Namespace 1 block size")).value(QStringLiteral("value")).toString(), QStringLiteral("512"));
        const auto wwid = fieldByKey(nvme, QStringLiteral("Namespace 1 WWID"));
        QCOMPARE(wwid.value(QStringLiteral("value")).toString(), QStringLiteral("eui.1234"));
        QVERIFY(wwid.value(QStringLiteral("sensitive")).toBool());
        QCOMPARE(fieldByKey(nvme, QStringLiteral("Temperature")).value(QStringLiteral("value")).toString(), QStringLiteral("38.0"));
        const auto smart = fieldByKey(nvme, QStringLiteral("SMART / health"));
        QCOMPARE(smart.value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QVERIFY(smart.contains(QStringLiteral("reason")));
        QVERIFY(!smart.value(QStringLiteral("reason")).toString().isEmpty());
    }

    void nvmeMalformedNamespaceSizeIsReadError()
    {
        QTemporaryDir root;
        writeFile(root, QStringLiteral("/sys/class/nvme/nvme0/nvme0n1/size"), "not-a-number\n");
        const auto nvme = deviceById(fixture(root), QStringLiteral("nvme:nvme0"));
        const auto sectors = fieldByKey(nvme, QStringLiteral("Namespace 1 sectors"));
        QCOMPARE(sectors.value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
        QCOMPARE(sectors.value(QStringLiteral("value")).toString(), QString());
        QVERIFY(!sectors.value(QStringLiteral("reason")).toString().isEmpty());
        const auto size = fieldByKey(nvme, QStringLiteral("Namespace 1 size"));
        QCOMPARE(size.value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
        QCOMPARE(size.value(QStringLiteral("value")).toString(), QString());
        QVERIFY(size.contains(QStringLiteral("reason")));
        QVERIFY(!size.value(QStringLiteral("reason")).toString().isEmpty());
        QCOMPARE(fieldByKey(nvme, QStringLiteral("SMART / health")).value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
    }

    void batteryHealthRequiresMatchingNonZeroValues()
    {
        QTemporaryDir root;
        const QString base = QStringLiteral("/sys/class/power_supply/BAT0");
        writeFile(root, base + QStringLiteral("/type"), "Battery\n");
        writeFile(root, base + QStringLiteral("/energy_full"), "50000000\n");
        writeFile(root, base + QStringLiteral("/energy_full_design"), "60000000\n");
        writeFile(root, base + QStringLiteral("/cycle_count"), "42\n");
        writeFile(root, base + QStringLiteral("/capacity"), "80\n");
        writeFile(root, base + QStringLiteral("/status"), "Discharging\n");
        writeFile(root, base + QStringLiteral("/power_now"), "12000000\n");
        writeFile(root, base + QStringLiteral("/voltage_now"), "11000000\n");
        writeFile(root, base + QStringLiteral("/voltage_min_design"), "10000000\n");
        const auto battery = deviceById(fixture(root), QStringLiteral("power:BAT0"));
        QCOMPARE(battery.value(QStringLiteral("category")).toString(), QStringLiteral("电池"));
        const auto health = fieldByKey(battery, QStringLiteral("Health percent"));
        QCOMPARE(health.value(QStringLiteral("status")).toString(), QStringLiteral("available"));
        QCOMPARE(health.value(QStringLiteral("value")).toString(), QStringLiteral("83.3"));
        QCOMPARE(health.value(QStringLiteral("unit")).toString(), QStringLiteral("%"));
        const double percent = health.value(QStringLiteral("rawValue")).toDouble();
        QVERIFY(percent > 83.3 && percent < 83.4);
        const auto energy = fieldByKey(battery, QStringLiteral("energy_full"));
        QCOMPARE(energy.value(QStringLiteral("value")).toString(), QStringLiteral("50.00"));
        QCOMPARE(energy.value(QStringLiteral("unit")).toString(), QStringLiteral("Wh"));
        QCOMPARE(fieldByKey(battery, QStringLiteral("power_now")).value(QStringLiteral("value")).toString(), QStringLiteral("12.000"));
        QCOMPARE(fieldByKey(battery, QStringLiteral("voltage_now")).value(QStringLiteral("value")).toString(), QStringLiteral("11.000"));
        QCOMPARE(fieldByKey(battery, QStringLiteral("cycle_count")).value(QStringLiteral("value")).toString(), QStringLiteral("42"));
        QCOMPARE(fieldByKey(battery, QStringLiteral("capacity")).value(QStringLiteral("value")).toString(), QStringLiteral("80"));
    }

    void batteryHealthRejectsZeroMismatchedAndMalformedData()
    {
        QTemporaryDir root;
        const QString zero = QStringLiteral("/sys/class/power_supply/BAT0");
        writeFile(root, zero + QStringLiteral("/type"), "Battery\n");
        writeFile(root, zero + QStringLiteral("/energy_full"), "50000000\n");
        writeFile(root, zero + QStringLiteral("/energy_full_design"), "0\n");
        writeFile(root, zero + QStringLiteral("/charge_full"), "2000000\n");
        writeFile(root, zero + QStringLiteral("/charge_full_design"), "2500000\n");
        const QString mismatch = QStringLiteral("/sys/class/power_supply/BAT1");
        writeFile(root, mismatch + QStringLiteral("/type"), "Battery\n");
        writeFile(root, mismatch + QStringLiteral("/energy_full"), "50000000\n");
        writeFile(root, mismatch + QStringLiteral("/charge_full_design"), "2500000\n");
        const QString malformed = QStringLiteral("/sys/class/power_supply/BAT2");
        writeFile(root, malformed + QStringLiteral("/type"), "Battery\n");
        writeFile(root, malformed + QStringLiteral("/energy_full"), "garbage\n");
        writeFile(root, malformed + QStringLiteral("/energy_full_design"), "60000000\n");
        const auto snapshot = fixture(root);
        const auto zeroBattery = deviceById(snapshot, QStringLiteral("power:BAT0"));
        QCOMPARE(fieldByKey(zeroBattery, QStringLiteral("Health percent")).value(QStringLiteral("value")).toString(), QStringLiteral("80.0"));
        QVERIFY(fieldByKey(deviceById(snapshot, QStringLiteral("power:BAT1")), QStringLiteral("Health percent")).isEmpty());
        const auto malformedBattery = deviceById(snapshot, QStringLiteral("power:BAT2"));
        QVERIFY(fieldByKey(malformedBattery, QStringLiteral("Health percent")).isEmpty());
        QCOMPARE(fieldByKey(malformedBattery, QStringLiteral("energy_full")).value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
    }

    void wirelessLinkQualityParsesProcNetWireless()
    {
        QTemporaryDir root;
        QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/sys/class/net/wlan0/wireless")));
        writeFile(root, QStringLiteral("/sys/class/net/wlan0/address"), "aa:bb:cc:dd:ee:ff\n");
        QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/sys/class/net/eth0")));
        writeFile(root, QStringLiteral("/proc/net/wireless"),
                  "Inter-| sta-|   Quality        |   Discarded packets               | Missed | WE\n"
                  " face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | 22\n"
                  "wlan0: 0000   67.  -52.  -90.        0      0      0      0      0        0        0\n");
        const auto snapshot = fixture(root);
        const auto wifi = deviceById(snapshot, QStringLiteral("wifi:wlan0"));
        QCOMPARE(wifi.value(QStringLiteral("category")).toString(), QStringLiteral("网络"));
        const auto mac = fieldByKey(wifi, QStringLiteral("MAC address"));
        QVERIFY(mac.value(QStringLiteral("sensitive")).toBool());
        QCOMPARE(mac.value(QStringLiteral("value")).toString(), QStringLiteral("aa:bb:cc:dd:ee:ff"));
        QCOMPARE(fieldByKey(wifi, QStringLiteral("Link quality")).value(QStringLiteral("value")).toString(), QStringLiteral("67"));
        QCOMPARE(fieldByKey(wifi, QStringLiteral("Signal level")).value(QStringLiteral("value")).toString(), QStringLiteral("-52"));
        QCOMPARE(fieldByKey(wifi, QStringLiteral("Signal level")).value(QStringLiteral("unit")).toString(), QStringLiteral("dBm"));
        QCOMPARE(fieldByKey(wifi, QStringLiteral("Noise level")).value(QStringLiteral("value")).toString(), QStringLiteral("-90"));
        QVERIFY(deviceById(snapshot, QStringLiteral("wifi:eth0")).isEmpty());
    }

    void wirelessWithoutStatisticsAndSentinelValuesDoNotInventReadings()
    {
        QTemporaryDir root;
        QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/sys/class/net/wlan0/wireless")));
        writeFile(root, QStringLiteral("/sys/class/net/wlan0/address"), "aa:bb:cc:dd:ee:ff\n");
        const auto missing = deviceById(fixture(root), QStringLiteral("wifi:wlan0"));
        for (const QString &key : {QStringLiteral("Link quality"), QStringLiteral("Signal level"),
                                   QStringLiteral("Noise level")}) {
            const auto field = fieldByKey(missing, key);
            QCOMPARE(field.value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
            QCOMPARE(field.value(QStringLiteral("value")).toString(), QString());
            QVERIFY(!field.value(QStringLiteral("reason")).toString().isEmpty());
        }
        writeFile(root, QStringLiteral("/proc/net/wireless"),
                  "wlan0: 0000   70.  -40.  -256.        0      0      0      0      0        0        0\n");
        const auto sentinel = deviceById(fixture(root), QStringLiteral("wifi:wlan0"));
        QCOMPARE(fieldByKey(sentinel, QStringLiteral("Signal level")).value(QStringLiteral("value")).toString(), QStringLiteral("-40"));
        const auto noise = fieldByKey(sentinel, QStringLiteral("Noise level"));
        QCOMPARE(noise.value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QCOMPARE(noise.value(QStringLiteral("value")).toString(), QString());
        QVERIFY(noise.value(QStringLiteral("reason")).toString().contains(QStringLiteral("sentinel")));
    }

    void usbTopologyIncludesInterfacesAndSensitiveSerial()
    {
        QTemporaryDir root;
        const QString base = QStringLiteral("/sys/bus/usb/devices/3-5");
        writeFile(root, base + QStringLiteral("/idVendor"), "3277\n");
        writeFile(root, base + QStringLiteral("/idProduct"), "0059\n");
        writeFile(root, base + QStringLiteral("/manufacturer"), "Shinetech\n");
        writeFile(root, base + QStringLiteral("/product"), "ASUS FHD webcam\n");
        writeFile(root, base + QStringLiteral("/serial"), "200901010001\n");
        writeFile(root, base + QStringLiteral("/speed"), "480\n");
        writeFile(root, base + QStringLiteral("/version"), " 2.01\n");
        writeFile(root, base + QStringLiteral("/maxchild"), "0\n");
        writeFile(root, base + QStringLiteral("/busnum"), "3\n");
        writeFile(root, base + QStringLiteral("/devnum"), "2\n");
        writeFile(root, base + QStringLiteral("/bNumInterfaces"), " 1\n");
        const QString interface = base + QStringLiteral("/3-5:1.0");
        writeFile(root, interface + QStringLiteral("/bInterfaceNumber"), "00\n");
        writeFile(root, interface + QStringLiteral("/bInterfaceClass"), "0e\n");
        writeFile(root, interface + QStringLiteral("/bInterfaceSubClass"), "01\n");
        writeFile(root, interface + QStringLiteral("/bInterfaceProtocol"), "00\n");
        writeFile(root, interface + QStringLiteral("/bNumEndpoints"), "01\n");
        writeFile(root, interface + QStringLiteral("/interface"), "Video\n");
        const auto snapshot = fixture(root);
        const auto usb = deviceById(snapshot, QStringLiteral("usb:3-5"));
        QCOMPARE(usb.value(QStringLiteral("name")).toString(), QStringLiteral("ASUS FHD webcam"));
        const auto serial = fieldByKey(usb, QStringLiteral("serial"));
        QVERIFY(serial.value(QStringLiteral("sensitive")).toBool());
        QCOMPARE(serial.value(QStringLiteral("value")).toString(), QStringLiteral("200901010001"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("speed")).value(QStringLiteral("value")).toString(), QStringLiteral("480"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("speed")).value(QStringLiteral("unit")).toString(), QStringLiteral("Mbps"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("version")).value(QStringLiteral("value")).toString(), QStringLiteral("2.01"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("maxchild")).value(QStringLiteral("value")).toString(), QStringLiteral("0"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("busnum")).value(QStringLiteral("value")).toString(), QStringLiteral("3"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("devnum")).value(QStringLiteral("value")).toString(), QStringLiteral("2"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("Interface 0 class")).value(QStringLiteral("value")).toString(), QStringLiteral("0e"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("Interface 0 subclass")).value(QStringLiteral("value")).toString(), QStringLiteral("01"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("Interface 0 protocol")).value(QStringLiteral("value")).toString(), QStringLiteral("00"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("Interface 0 endpoints")).value(QStringLiteral("value")).toString(), QStringLiteral("01"));
        QCOMPARE(fieldByKey(usb, QStringLiteral("Interface 0 name")).value(QStringLiteral("value")).toString(), QStringLiteral("Video"));
        QVERIFY(deviceById(snapshot, QStringLiteral("usb:3-5:1.0")).isEmpty());
    }

    void pciDepthIncludesLinksIrqAndDriver()
    {
        QTemporaryDir root;
        const QString base = QStringLiteral("/sys/bus/pci/devices/0000:01:00.0");
        writeFile(root, base + QStringLiteral("/vendor"), "0x10de\n");
        writeFile(root, base + QStringLiteral("/device"), "0x2704\n");
        writeFile(root, base + QStringLiteral("/subsystem_vendor"), "0x1043\n");
        writeFile(root, base + QStringLiteral("/subsystem_device"), "0x8765\n");
        writeFile(root, base + QStringLiteral("/class"), "0x030000\n");
        writeFile(root, base + QStringLiteral("/revision"), "0xa1\n");
        writeFile(root, base + QStringLiteral("/irq"), "255\n");
        writeFile(root, base + QStringLiteral("/local_cpulist"), "0-15\n");
        writeFile(root, base + QStringLiteral("/numa_node"), "0\n");
        writeFile(root, base + QStringLiteral("/current_link_speed"), "16.0 GT/s\n");
        writeFile(root, base + QStringLiteral("/current_link_width"), "16\n");
        writeFile(root, base + QStringLiteral("/max_link_speed"), "32.0 GT/s\n");
        writeFile(root, base + QStringLiteral("/max_link_width"), "16\n");
        writeFile(root, base + QStringLiteral("/power_state"), "D0\n");
        writeFile(root, base + QStringLiteral("/d3cold_allowed"), "1\n");
        writeFile(root, base + QStringLiteral("/label"), "fixture gpu\n");
        writeFile(root, base + QStringLiteral("/index"), "1\n");
        writeFile(root, base + QStringLiteral("/resource"),
                  "0x00000000c0000000 0x00000000c0ffffff 0x00040200\n"
                  "0x00000000c1000000 0x00000000c1ffffff 0x00040200\n");
        writeFile(root, base + QStringLiteral("/msi_irqs/255"), "1\n");
        QVERIFY(QDir().mkpath(root.path() + QStringLiteral("/sys/bus/pci/drivers/fixture-drv")));
        QVERIFY(QFile::link(root.path() + QStringLiteral("/sys/bus/pci/drivers/fixture-drv"),
                            root.path() + base + QStringLiteral("/driver")));
        const auto pci = deviceById(fixture(root), QStringLiteral("pci:0000:01:00.0"));
        QCOMPARE(pci.value(QStringLiteral("category")).toString(), QStringLiteral("显卡"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("subsystem_vendor")).value(QStringLiteral("value")).toString(), QStringLiteral("0x1043"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("subsystem_device")).value(QStringLiteral("value")).toString(), QStringLiteral("0x8765"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("irq")).value(QStringLiteral("value")).toString(), QStringLiteral("255"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("local_cpulist")).value(QStringLiteral("value")).toString(), QStringLiteral("0-15"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("numa_node")).value(QStringLiteral("value")).toString(), QStringLiteral("0"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("current_link_speed")).value(QStringLiteral("value")).toString(), QStringLiteral("16.0 GT/s"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("current_link_width")).value(QStringLiteral("value")).toString(), QStringLiteral("16"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("max_link_speed")).value(QStringLiteral("value")).toString(), QStringLiteral("32.0 GT/s"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("max_link_width")).value(QStringLiteral("value")).toString(), QStringLiteral("16"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("Driver")).value(QStringLiteral("value")).toString(), QStringLiteral("fixture-drv"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("power_state")).value(QStringLiteral("value")).toString(), QStringLiteral("D0"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("d3cold_allowed")).value(QStringLiteral("value")).toString(), QStringLiteral("1"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("label")).value(QStringLiteral("value")).toString(), QStringLiteral("fixture gpu"));
        QCOMPARE(fieldByKey(pci, QStringLiteral("index")).value(QStringLiteral("value")).toString(), QStringLiteral("1"));
        QVERIFY(fieldByKey(pci, QStringLiteral("resource")).value(QStringLiteral("value")).toString().contains(QStringLiteral("0x00000000c0000000")));
        QCOMPARE(fieldByKey(pci, QStringLiteral("MSI IRQs")).value(QStringLiteral("value")).toString(), QStringLiteral("255"));
    }

    void batterySignedMeasurementsRemainValid()
    {
        QTemporaryDir root;
        const QString base = QStringLiteral("/sys/class/power_supply/BAT0");
        writeFile(root, base + QStringLiteral("/type"), "Battery\n");
        writeFile(root, base + QStringLiteral("/temp"), "-50\n");
        writeFile(root, base + QStringLiteral("/current_now"), "-1200000\n");
        writeFile(root, base + QStringLiteral("/power_now"), "-3000000\n");
        writeFile(root, base + QStringLiteral("/energy_now"), "-1000000\n");
        writeFile(root, base + QStringLiteral("/voltage_now"), "-11000000\n");
        const auto battery = deviceById(fixture(root), QStringLiteral("power:BAT0"));
        const auto temperature = fieldByKey(battery, QStringLiteral("temp"));
        QCOMPARE(temperature.value(QStringLiteral("status")).toString(), QStringLiteral("available"));
        QCOMPARE(temperature.value(QStringLiteral("value")).toString(), QStringLiteral("-5.0"));
        QCOMPARE(temperature.value(QStringLiteral("unit")).toString(), QStringLiteral("°C"));
        QCOMPARE(temperature.value(QStringLiteral("rawValue")).toDouble(), -5.0);
        const auto current = fieldByKey(battery, QStringLiteral("current_now"));
        QCOMPARE(current.value(QStringLiteral("status")).toString(), QStringLiteral("available"));
        QCOMPARE(current.value(QStringLiteral("value")).toString(), QStringLiteral("-1.200"));
        QCOMPARE(current.value(QStringLiteral("unit")).toString(), QStringLiteral("A"));
        QVERIFY(qAbs(current.value(QStringLiteral("rawValue")).toDouble() + 1.2) < 1e-9);
        const auto power = fieldByKey(battery, QStringLiteral("power_now"));
        QCOMPARE(power.value(QStringLiteral("status")).toString(), QStringLiteral("available"));
        QCOMPARE(power.value(QStringLiteral("value")).toString(), QStringLiteral("-3.000"));
        QCOMPARE(power.value(QStringLiteral("unit")).toString(), QStringLiteral("W"));
        QVERIFY(qAbs(power.value(QStringLiteral("rawValue")).toDouble() + 3.0) < 1e-9);
        const auto energy = fieldByKey(battery, QStringLiteral("energy_now"));
        QCOMPARE(energy.value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
        QCOMPARE(energy.value(QStringLiteral("value")).toString(), QString());
        const auto voltage = fieldByKey(battery, QStringLiteral("voltage_now"));
        QCOMPARE(voltage.value(QStringLiteral("status")).toString(), QStringLiteral("read-error"));
        QCOMPARE(voltage.value(QStringLiteral("value")).toString(), QString());
    }
};

QTEST_GUILESS_MAIN(InventoryTest)
#include "tst_inventory.moc"
