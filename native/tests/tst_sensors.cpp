#include "../core/SensorService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSemaphore>
#include <QThreadPool>
#include <QScopeGuard>
#include <QtConcurrent/QtConcurrentRun>
#include <QTemporaryDir>
#include <QtTest>

namespace {
bool writeFile(const QString &path, const QByteArray &data)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}
QVariantMap reading(const neko::SensorService &service, const QString &id)
{
    for (const auto &entry : service.readings()) {
        const auto item = entry.toMap();
        if (item.value(QStringLiteral("id")).toString() == id) return item;
    }
    return {};
}
}

class SensorServiceTest final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void readsLiveMemory();
    void unavailableRootsDoNotInventReadings();
    void cpuRequiresTwoSamplesAndPreservesWideCounters();
    void sameNameChipsAndSymlinksRemainDistinct();
    void removalClearsSummaryAndStatistics();
    void pauseKeepsTemperatureObservationButFreezesUi();
    void resetClearsStatisticsAndHistory();
    void historyAndStatisticsAreBounded();
    void counterRatesUseElapsedTimeAndResetMissingDevices();
    void malformedDataIsUnavailable();
    void asyncSamplingIsDeferredAndCoalescesRequests();
    void destructionWaitsForInFlightCollection();
    void pauseStillReportsUnavailableTemperature();
    void faultedAndDisabledChannelsAreUnavailable();
    void cpuPowerUsesEnergyDeltasAndWrapRange();
    void readingsCarryDeviceIdentity();
    void historyLimitIsConfigurable();
    void perCoreFrequencyIsAggregated();
private:
    QTemporaryDir settingsDir_;
};

void SensorServiceTest::initTestCase()
{
    QVERIFY(settingsDir_.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("NekoScopeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SensorService"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir_.path());
}

void SensorServiceTest::readsLiveMemory()
{
    neko::SensorService service;
    service.sample();
    const auto memory = reading(service, QStringLiteral("memory.used"));
    QVERIFY(!memory.isEmpty());
    QVERIFY(memory.value(QStringLiteral("value")).toDouble() > 0.0);
    QCOMPARE(memory.value(QStringLiteral("status")).toString(), QStringLiteral("available"));
    QVERIFY(!memory.value(QStringLiteral("source")).toString().isEmpty());
    QVERIFY(service.summary().value(QStringLiteral("memoryTotalGiB")).toDouble() > 0.0);
}

void SensorServiceTest::unavailableRootsDoNotInventReadings()
{
    QTemporaryDir root;
    neko::SensorService service(root.path(), root.path());
    service.sample();
    QVERIFY(service.readings().isEmpty());
    QVERIFY(service.cpuHistory().isEmpty());
    QCOMPARE(service.summary().value(QStringLiteral("memoryTotalGiB")).toDouble(), 0.0);
    QCOMPARE(service.summary().value(QStringLiteral("cpuTemperature")).toDouble(), -1.0);
}

void SensorServiceTest::cpuRequiresTwoSamplesAndPreservesWideCounters()
{
    QTemporaryDir root;
    QVERIFY(writeFile(root.filePath("stat"), "cpu 5000000000 0 0 5000000000 0 0 0 0 100 0\ncpu0 5000000000 0 0 5000000000 0 0 0 0 100 0\n"));
    neko::SensorService service(root.path(), root.path());
    service.sample();
    QVERIFY(reading(service, "cpu.usage").isEmpty());
    QVERIFY(service.cpuHistory().isEmpty());
    QVERIFY(writeFile(root.filePath("stat"), "cpu 5000000050 0 0 5000000050 0 0 0 0 150 0\ncpu0 5000000050 0 0 5000000050 0 0 0 0 150 0\n"));
    service.sample();
    // guest time is included in user time already; only 50 busy / 100 total.
    QCOMPARE(reading(service, "cpu.usage").value("value").toDouble(), 50.0);
    QCOMPARE(reading(service, "cpu.core.0.usage").value("value").toDouble(), 50.0);
    QCOMPARE(service.cpuHistory().size(), 1);
}

void SensorServiceTest::sameNameChipsAndSymlinksRemainDistinct()
{
    QTemporaryDir root;
    QVERIFY(writeFile(root.filePath("devices/a/hwmon/hwmon0/name"), "coretemp\n"));
    QVERIFY(writeFile(root.filePath("devices/a/hwmon/hwmon0/temp1_input"), "42000\n"));
    QVERIFY(writeFile(root.filePath("devices/b/hwmon/hwmon1/name"), "coretemp\n"));
    QVERIFY(writeFile(root.filePath("devices/b/hwmon/hwmon1/temp1_input"), "65000\n"));
    QVERIFY(QDir().mkpath(root.filePath("class/hwmon")));
    QVERIFY(QFile::link(root.filePath("devices/a/hwmon/hwmon0"), root.filePath("class/hwmon/hwmon0")));
    QVERIFY(QFile::link(root.filePath("devices/b/hwmon/hwmon1"), root.filePath("class/hwmon/hwmon1")));
    neko::SensorService service(root.filePath("absent"), root.path());
    service.sample();
    QSet<QString> ids;
    QList<double> temperatures;
    for (const auto &entry : service.readings()) {
        const auto item = entry.toMap();
        const auto id = item.value("id").toString();
        QVERIFY(!ids.contains(id));
        ids.insert(id);
        if (id.startsWith("hwmon.")) temperatures.append(item.value("value").toDouble());
    }
    std::sort(temperatures.begin(), temperatures.end());
    QCOMPARE(temperatures, QList<double>({42.0, 65.0}));
    // Protection uses the hottest recognized CPU channel, including on multi-socket hosts.
    QCOMPARE(service.summary().value("cpuTemperature").toDouble(), 65.0);
}

void SensorServiceTest::removalClearsSummaryAndStatistics()
{
    QTemporaryDir root;
    QVERIFY(writeFile(root.filePath("meminfo"), "MemTotal: 1000 kB\nMemAvailable: 500 kB\n"));
    neko::SensorService service(root.path(), root.path());
    service.sample();
    QVERIFY(service.summary().value("memoryUsage").toDouble() > 0.0);
    QVERIFY(QFile::remove(root.filePath("meminfo")));
    service.sample();
    QVERIFY(service.readings().isEmpty());
    QCOMPARE(service.summary().value("memoryUsage").toDouble(), -1.0);
    QCOMPARE(service.summary().value("memoryTotalGiB").toDouble(), 0.0);
    QVERIFY(writeFile(root.filePath("meminfo"), "MemTotal: 1000 kB\nMemAvailable: 200 kB\n"));
    service.sample();
    const auto usage = reading(service, "memory.usage");
    QCOMPARE(usage.value("min").toDouble(), 80.0);
    QCOMPARE(usage.value("max").toDouble(), 80.0);
    QCOMPARE(usage.value("avg").toDouble(), 80.0);
}

void SensorServiceTest::pauseKeepsTemperatureObservationButFreezesUi()
{
    QTemporaryDir root;
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon0/name"), "coretemp\n"));
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon0/temp1_input"), "42000\n"));
    neko::SensorService service(root.filePath("proc"), root.path());
    QSignalSpy observed(&service, &neko::SensorService::temperatureObserved);
    service.sample();
    const auto initialReadings = service.readings();
    const auto initialHistory = service.temperatureHistory();
    QVERIFY(!initialHistory.isEmpty());
    service.setPaused(true);
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon0/temp1_input"), "55000\n"));
    service.sample();
    QCOMPARE(observed.count(), 2);
    QCOMPARE(observed.last().first().toDouble(), 55.0);
    QCOMPARE(service.readings(), initialReadings);
    QCOMPARE(service.temperatureHistory(), initialHistory);
}

void SensorServiceTest::resetClearsStatisticsAndHistory()
{
    QTemporaryDir root;
    QVERIFY(writeFile(root.filePath("meminfo"), "MemTotal: 1000 kB\nMemAvailable: 500 kB\n"));
    neko::SensorService service(root.path(), root.path());
    service.sample();
    QVERIFY(writeFile(root.filePath("meminfo"), "MemTotal: 1000 kB\nMemAvailable: 200 kB\n"));
    service.sample();
    service.setPaused(true);
    service.resetStatistics();
    QVERIFY(service.cpuHistory().isEmpty());
    QVERIFY(service.temperatureHistory().isEmpty());
    for (const auto &entry : service.readings()) {
        const auto item = entry.toMap();
        QCOMPARE(item.value("min").toDouble(), item.value("value").toDouble());
        QCOMPARE(item.value("max").toDouble(), item.value("value").toDouble());
        QCOMPARE(item.value("avg").toDouble(), item.value("value").toDouble());
    }
}

void SensorServiceTest::historyAndStatisticsAreBounded()
{
    QTemporaryDir root;
    const auto channel = root.filePath("class/hwmon/hwmon0/temp1_input");
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon0/name"), "coretemp\n"));
    neko::SensorService service(root.filePath("proc"), root.path());
    QVERIFY(writeFile(channel, "1000\n"));
    service.sample();
    for (int i = 0; i < 100; ++i) {
        QVERIFY(writeFile(channel, "42000\n"));
        service.sample();
    }
    QVERIFY(service.temperatureHistory().size() <= 100);
    const auto temperature = reading(service, "cpu.temperature");
    QCOMPARE(temperature.value("min").toDouble(), 42.0);
    QCOMPARE(temperature.value("max").toDouble(), 42.0);
    QCOMPARE(temperature.value("avg").toDouble(), 42.0);
}

void SensorServiceTest::counterRatesUseElapsedTimeAndResetMissingDevices()
{
    QTemporaryDir root;
    const auto net = root.filePath("class/net/eth0/statistics/rx_bytes");
    const auto disk = root.filePath("diskstats");
    QVERIFY(writeFile(net, "9007199254740992\n"));
    QVERIFY(writeFile(disk, "8 0 sda 0 0 100 0 0 0 200 0 0 0 0\n"));
    neko::SensorCollector collector(root.path(), root.path());
    const auto start = neko::SensorCollector::Clock::time_point{};
    auto snapshot = collector.collect(start);
    QVERIFY(!snapshot.values.contains("net.eth0.rx.rate"));
    QVERIFY(!snapshot.values.contains("disk.sda.read.rate"));
    QVERIFY(writeFile(net, "9007199254745088\n"));
    QVERIFY(writeFile(disk, "8 0 sda 0 0 108 0 0 0 216 0 0 0 0\n"));
    snapshot = collector.collect(start + std::chrono::seconds(2));
    QCOMPARE(snapshot.values.value("net.eth0.rx.rate").toDouble(), 2.0);
    QCOMPARE(snapshot.values.value("disk.sda.read.rate").toDouble(), 2.0);
    QCOMPARE(snapshot.values.value("disk.sda.write.rate").toDouble(), 4.0);
    QVERIFY(writeFile(net, "9007199254746112\n"));
    snapshot = collector.collect(start + std::chrono::milliseconds(2500));
    QCOMPARE(snapshot.values.value("net.eth0.rx.rate").toDouble(), 2.0);
    QVERIFY(QFile::remove(net));
    snapshot = collector.collect(start + std::chrono::seconds(3));
    QVERIFY(!snapshot.values.contains("net.eth0.rx.rate"));
    QVERIFY(writeFile(net, "20\n"));
    snapshot = collector.collect(start + std::chrono::seconds(4));
    QVERIFY(!snapshot.values.contains("net.eth0.rx.rate"));
    QVERIFY(writeFile(net, "1044\n"));
    snapshot = collector.collect(start + std::chrono::seconds(5));
    QCOMPARE(snapshot.values.value("net.eth0.rx.rate").toDouble(), 1.0);
    // A counter reset is another baseline, never a negative or huge throughput.
    QVERIFY(writeFile(net, "1\n"));
    snapshot = collector.collect(start + std::chrono::seconds(6));
    QVERIFY(!snapshot.values.contains("net.eth0.rx.rate"));
}

void SensorServiceTest::malformedDataIsUnavailable()
{
    QTemporaryDir root;
    QVERIFY(writeFile(root.filePath("meminfo"), "MemTotal: 0 kB\nMemAvailable: 100 kB\n"));
    QVERIFY(writeFile(root.filePath("stat"), "cpu 100 invalid 0 100\n"));
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon0/temp1_input"), "NaN\n"));
    neko::SensorService service(root.path(), root.path());
    service.sample();
    QVERIFY(service.readings().isEmpty());
    QCOMPARE(service.summary().value("memoryUsage").toDouble(), -1.0);
}

void SensorServiceTest::asyncSamplingIsDeferredAndCoalescesRequests()
{
    QTemporaryDir root;
    QVERIFY(writeFile(root.filePath("meminfo"), "MemTotal: 1000 kB\nMemAvailable: 500 kB\n"));
    auto *pool = QThreadPool::globalInstance();
    pool->waitForDone();
    const int oldLimit = pool->maxThreadCount();
    pool->setMaxThreadCount(1);
    QSemaphore entered, release;
    auto blocker = QtConcurrent::run([&] { entered.release(); release.acquire(); });
    neko::SensorService service(root.path(), root.path());
    const auto unblock = qScopeGuard([&] { release.release(); blocker.waitForFinished(); pool->setMaxThreadCount(oldLimit); });
    QVERIFY(entered.tryAcquire(1, 3000));
    QSignalSpy finished(&service, &neko::SensorService::sampleFinished);
    service.requestSample();
    QVERIFY(service.busy());
    for (int i = 0; i < 20; ++i) service.requestSample();
    // Also reject the synchronous entry point until the async snapshot is applied.
    service.sample();
    QVERIFY(service.readings().isEmpty());
    QCOMPARE(finished.count(), 0);
    release.release();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
    QVERIFY(!service.busy());
    QCOMPARE(service.summary().value("memoryUsage").toDouble(), 50.0);
    service.requestSample();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 3000);
}

void SensorServiceTest::destructionWaitsForInFlightCollection()
{
    QTemporaryDir root;
    QVERIFY(writeFile(root.filePath("meminfo"), "MemTotal: 1000 kB\nMemAvailable: 500 kB\n"));
    // Repeated immediate closes exercise both queued and already running work.
    for (int i = 0; i < 20; ++i) {
        neko::SensorService service(root.path(), root.path());
        service.requestSample();
    }
    QCoreApplication::processEvents();
}

void SensorServiceTest::pauseStillReportsUnavailableTemperature()
{
    QTemporaryDir root;
    const auto channel = root.filePath("class/hwmon/hwmon0/temp1_input");
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon0/name"), "coretemp\n"));
    QVERIFY(writeFile(channel, "-5000\n"));
    neko::SensorService service(root.filePath("proc"), root.path());
    QSignalSpy observed(&service, &neko::SensorService::temperatureObserved);
    QSignalSpy sampled(&service, &neko::SensorService::temperatureSampled);
    service.sample();
    QCOMPARE(observed.count(), 1);
    QCOMPARE(observed.first().first().toDouble(), -5.0);
    QCOMPARE(sampled.first().at(0).toBool(), true);
    service.setPaused(true);
    QVERIFY(QFile::remove(channel));
    service.sample();
    QCOMPARE(observed.count(), 1);
    QCOMPARE(sampled.count(), 2);
    QCOMPARE(sampled.last().at(0).toBool(), false);
}

void SensorServiceTest::faultedAndDisabledChannelsAreUnavailable()
{
    QTemporaryDir root;
    const auto base = root.filePath("class/hwmon/hwmon0");
    QVERIFY(writeFile(base + "/name", "coretemp\n"));
    QVERIFY(writeFile(base + "/temp1_input", "42000\n"));
    QVERIFY(writeFile(base + "/temp1_fault", "1\n"));
    QVERIFY(writeFile(base + "/temp2_input", "55000\n"));
    QVERIFY(writeFile(base + "/temp2_enable", "0\n"));
    neko::SensorService service(root.filePath("proc"), root.path());
    service.sample();
    QVERIFY(service.readings().isEmpty());
    QVERIFY(writeFile(base + "/temp1_fault", "0\n"));
    service.sample();
    QCOMPARE(service.summary().value("cpuTemperature").toDouble(), 42.0);
}

void SensorServiceTest::cpuPowerUsesEnergyDeltasAndWrapRange()
{
    QTemporaryDir root;
    const auto base = root.filePath("class/powercap/intel-rapl:0");
    QVERIFY(writeFile(base + "/name", "package-0\n"));
    QVERIFY(writeFile(base + "/energy_uj", "1000000\n"));
    QVERIFY(writeFile(base + "/max_energy_range_uj", "10000000\n"));
    neko::SensorCollector collector(root.filePath("proc"), root.path());
    const auto start = neko::SensorCollector::Clock::time_point{};
    auto snapshot = collector.collect(start);
    QVERIFY(!snapshot.values.contains("cpu.power"));
    QVERIFY(writeFile(base + "/energy_uj", "9000000\n"));
    snapshot = collector.collect(start + std::chrono::seconds(2));
    QCOMPARE(snapshot.values.value("cpu.power").toDouble(), 4.0);
    QVERIFY(writeFile(base + "/energy_uj", "1000000\n"));
    snapshot = collector.collect(start + std::chrono::seconds(3));
    QCOMPARE(snapshot.values.value("cpu.power").toDouble(), 2.0);
    // A decrease without a published wrap range cannot be interpreted safely.
    QVERIFY(QFile::remove(base + "/max_energy_range_uj"));
    QVERIFY(writeFile(base + "/energy_uj", "100\n"));
    snapshot = collector.collect(start + std::chrono::seconds(4));
    QVERIFY(!snapshot.values.contains("cpu.power"));
}

QTEST_GUILESS_MAIN(SensorServiceTest)
void SensorServiceTest::readingsCarryDeviceIdentity()
{
    QTemporaryDir root;
    QVERIFY(writeFile(root.filePath("stat"),
                      "cpu 1000 0 0 1000 0 0 0 0 0 0\ncpu0 1000 0 0 1000 0 0 0 0 0 0\n"));
    QVERIFY(writeFile(root.filePath("meminfo"), "MemTotal: 1000 kB\nMemAvailable: 500 kB\n"));
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon0/name"), "coretemp\n"));
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon0/temp1_input"), "42000\n"));
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon1/name"), "fixture-chip\n"));
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon1/temp1_input"), "31000\n"));
    QVERIFY(writeFile(root.filePath("class/power_supply/BAT0/type"), "Battery\n"));
    QVERIFY(writeFile(root.filePath("class/power_supply/BAT0/capacity"), "80\n"));
    QVERIFY(writeFile(root.filePath("class/drm/card0/device/gpu_busy_percent"), "42\n"));
    neko::SensorService service(root.path(), root.path());
    service.sample();
    QVERIFY(writeFile(root.filePath("stat"),
                      "cpu 1050 0 0 1050 0 0 0 0 0 0\ncpu0 1050 0 0 1050 0 0 0 0 0 0\n"));
    service.sample();

    const auto cpu = reading(service, QStringLiteral("cpu.usage"));
    QCOMPARE(cpu.value(QStringLiteral("deviceId")).toString(), QStringLiteral("cpu"));
    QCOMPARE(cpu.value(QStringLiteral("device")).toString(), QStringLiteral("CPU"));
    QCOMPARE(reading(service, QStringLiteral("memory.used")).value(QStringLiteral("deviceId")).toString(),
             QStringLiteral("memory"));
    QCOMPARE(reading(service, QStringLiteral("battery.BAT0.charge")).value(QStringLiteral("deviceId")).toString(),
             QStringLiteral("battery"));
    QCOMPARE(reading(service, QStringLiteral("gpu.card0.usage")).value(QStringLiteral("deviceId")).toString(),
             QStringLiteral("gpu"));

    QVariantMap cpuChip, unknownChip;
    for (const auto &entry : service.readings()) {
        const auto item = entry.toMap();
        if (item.value(QStringLiteral("group")).toString() != QStringLiteral("Temperature")) continue;
        if (item.value(QStringLiteral("name")).toString().startsWith(QStringLiteral("coretemp"))) cpuChip = item;
        if (item.value(QStringLiteral("name")).toString().startsWith(QStringLiteral("fixture-chip"))) unknownChip = item;
    }
    QVERIFY(!cpuChip.isEmpty());
    QCOMPARE(cpuChip.value(QStringLiteral("deviceId")).toString(), QStringLiteral("cpu"));
    QCOMPARE(cpuChip.value(QStringLiteral("device")).toString(), QStringLiteral("CPU"));
    QVERIFY(!unknownChip.isEmpty());
    QVERIFY(unknownChip.value(QStringLiteral("deviceId")).toString().startsWith(QStringLiteral("hwmon:")));
    QCOMPARE(unknownChip.value(QStringLiteral("device")).toString(), QStringLiteral("fixture-chip"));
}

void SensorServiceTest::historyLimitIsConfigurable()
{
    QTemporaryDir root;
    const auto channel = root.filePath("class/hwmon/hwmon0/temp1_input");
    QVERIFY(writeFile(root.filePath("class/hwmon/hwmon0/name"), "coretemp\n"));
    neko::SensorService service(root.path(), root.path());
    service.setHistoryLimit(10);
    QCOMPARE(service.historyLimit(), 10);
    for (int i = 0; i < 15; ++i) {
        QVERIFY(writeFile(channel, QByteArray::number(40000 + i * 1000) + "\n"));
        service.sample();
    }
    QCOMPARE(service.temperatureHistory().size(), 10);
    service.setHistoryLimit(20);
    QCOMPARE(service.historyLimit(), 20);
    service.setHistoryLimit(10000);
    QCOMPARE(service.historyLimit(), 600);
}

void SensorServiceTest::perCoreFrequencyIsAggregated()
{
    QTemporaryDir root;
    QVERIFY(writeFile(root.filePath("devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"), "1000000\n"));
    QVERIFY(writeFile(root.filePath("devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq"), "4000000\n"));
    QVERIFY(writeFile(root.filePath("devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq"), "400000\n"));
    QVERIFY(writeFile(root.filePath("devices/system/cpu/cpu1/cpufreq/scaling_cur_freq"), "3000000\n"));
    QVERIFY(writeFile(root.filePath("devices/system/cpu/cpu1/cpufreq/cpuinfo_max_freq"), "4800000\n"));
    QVERIFY(writeFile(root.filePath("devices/system/cpu/cpu1/cpufreq/cpuinfo_min_freq"), "400000\n"));
    neko::SensorService service(root.path(), root.path());
    service.sample();

    const auto core0 = reading(service, QStringLiteral("cpu.frequency.core.0"));
    QCOMPARE(core0.value(QStringLiteral("value")).toDouble(), 1000.0);
    QCOMPARE(core0.value(QStringLiteral("unit")).toString(), QStringLiteral("MHz"));
    QCOMPARE(core0.value(QStringLiteral("group")).toString(), QStringLiteral("Frequency"));
    QCOMPARE(core0.value(QStringLiteral("deviceId")).toString(), QStringLiteral("cpu"));
    QCOMPARE(reading(service, QStringLiteral("cpu.frequency.core.1")).value(QStringLiteral("value")).toDouble(), 3000.0);
    QCOMPARE(reading(service, QStringLiteral("cpu.frequency")).value(QStringLiteral("value")).toDouble(), 2000.0);
    QCOMPARE(reading(service, QStringLiteral("cpu.frequency.max")).value(QStringLiteral("value")).toDouble(), 4800.0);
    QCOMPARE(reading(service, QStringLiteral("cpu.frequency.min")).value(QStringLiteral("value")).toDouble(), 400.0);
    QCOMPARE(service.summary().value(QStringLiteral("cpuFrequencyGHz")).toDouble(), 2.0);
    QCOMPARE(service.summary().value(QStringLiteral("cpuFrequencyMaxGHz")).toDouble(), 4.8);
    QCOMPARE(service.summary().value(QStringLiteral("cpuFrequencyMinGHz")).toDouble(), 0.4);
}

#include "tst_sensors.moc"
