#include "../core/SensorService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class SensorServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readsLiveMemory();
    void unavailableRootsDoNotInventReadings();
    void pauseKeepsTemperatureObservationButFreezesUi();
    void resetClearsStatisticsAndHistory();

private:
    QTemporaryDir settingsDir_;
};

void SensorServiceTest::initTestCase()
{
    QVERIFY(settingsDir_.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("NekoScopeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SensorService"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    qputenv("XDG_CONFIG_HOME", settingsDir_.path().toLocal8Bit());
}

void SensorServiceTest::readsLiveMemory()
{
    neko::SensorService service;
    service.sample();

    bool foundMemory = false;
    for (const auto &entry : service.readings()) {
        const auto reading = entry.toMap();
        if (reading.value(QStringLiteral("id")).toString() == QStringLiteral("memory.used")) {
            foundMemory = true;
            QVERIFY(reading.value(QStringLiteral("value")).toDouble() > 0.0);
            QCOMPARE(reading.value(QStringLiteral("status")).toString(), QStringLiteral("available"));
            QVERIFY(!reading.value(QStringLiteral("source")).toString().isEmpty());
        }
    }
    QVERIFY(foundMemory);
    QVERIFY(service.summary().value(QStringLiteral("memoryTotalGiB")).toDouble() > 0.0);
}

void SensorServiceTest::unavailableRootsDoNotInventReadings()
{
    QTemporaryDir emptyRoot;
    QVERIFY(emptyRoot.isValid());
    neko::SensorService service(emptyRoot.path(), emptyRoot.path());
    service.sample();

    for (const auto &entry : service.readings()) {
        const auto reading = entry.toMap();
        QVERIFY(reading.value(QStringLiteral("id")).toString() != QStringLiteral("memory.used"));
        QVERIFY(reading.value(QStringLiteral("value")).toDouble() != 0.0);
    }
    QCOMPARE(service.summary().value(QStringLiteral("memoryTotalGiB")).toDouble(), 0.0);
    QCOMPARE(service.summary().value(QStringLiteral("cpuTemperature")).toDouble(), -1.0);
}

void SensorServiceTest::pauseKeepsTemperatureObservationButFreezesUi()
{
    QTemporaryDir emptyRoot;
    const auto proc = emptyRoot.filePath(QStringLiteral("proc"));
    const auto sys = emptyRoot.filePath(QStringLiteral("sys"));
    QVERIFY(QDir().mkpath(proc));
    QVERIFY(QDir().mkpath(sys + QStringLiteral("/class/hwmon/hwmon0")));
    QFile meminfo(proc + QStringLiteral("/meminfo"));
    QVERIFY(meminfo.open(QIODevice::WriteOnly));
    meminfo.write("MemTotal: 1000 kB\nMemAvailable: 500 kB\n");
    meminfo.close();
    QFile temp(sys + QStringLiteral("/class/hwmon/hwmon0/temp1_input"));
    QVERIFY(temp.open(QIODevice::WriteOnly));
    temp.write("42000\n");
    temp.close();
    QFile name(sys + QStringLiteral("/class/hwmon/hwmon0/name"));
    QVERIFY(name.open(QIODevice::WriteOnly));
    name.write("coretemp\n");
    name.close();

    neko::SensorService service(proc, sys);
    QSignalSpy observed(&service, &neko::SensorService::temperatureObserved);
    service.sample();
    const auto initialReadings = service.readings();
    const auto initialHistory = service.temperatureHistory();
    QVERIFY(!initialHistory.isEmpty());

    service.setPaused(true);
    service.sample();
    QCOMPARE(observed.count(), 2);
    QCOMPARE(service.readings(), initialReadings);
    QCOMPARE(service.temperatureHistory(), initialHistory);
}

void SensorServiceTest::resetClearsStatisticsAndHistory()
{
    neko::SensorService service;
    service.sample();
    QVERIFY(!service.cpuHistory().isEmpty());
    service.resetStatistics();
    QVERIFY(service.cpuHistory().isEmpty());
    QVERIFY(service.temperatureHistory().isEmpty());
    for (const auto &entry : service.readings()) {
        const auto reading = entry.toMap();
        QCOMPARE(reading.value(QStringLiteral("min")).toDouble(), reading.value(QStringLiteral("value")).toDouble());
        QCOMPARE(reading.value(QStringLiteral("max")).toDouble(), reading.value(QStringLiteral("value")).toDouble());
        QCOMPARE(reading.value(QStringLiteral("avg")).toDouble(), reading.value(QStringLiteral("value")).toDouble());
    }
}

QTEST_GUILESS_MAIN(SensorServiceTest)
#include "tst_sensors.moc"
