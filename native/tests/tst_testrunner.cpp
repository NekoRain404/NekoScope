#include "../core/TestRunner.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QtTest>
#include <memory>
#ifdef Q_OS_UNIX
#include <csignal>
#include <unistd.h>
#endif

using neko::TestRunner;

namespace {
bool writeFile(const QString &path, const QByteArray &contents)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}
QString fixture(QTemporaryDir &dir, const QByteArray &body, const QString &name = QStringLiteral("fixture"))
{
    const auto path = dir.filePath(name);
    if (!writeFile(path, QByteArray("#!/usr/bin/python3\nimport os, sys, time, signal\n") + body)) return {};
    if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) return {};
    return path;
}
bool processAlive(qint64 pid)
{
#ifdef Q_OS_LINUX
    QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly)) return false;
    const auto text = stat.readAll();
    const auto end = text.lastIndexOf(')');
    return end >= 0 && text.mid(end + 2, 1) != "Z";
#else
    Q_UNUSED(pid);
    return false;
#endif
}
}

class TestTestRunner final : public QObject
{
    Q_OBJECT
private slots:
    void stressCommandsAreBounded()
    {
        const auto command = TestRunner::stressCommand("combined", 80, 16, 3600, 4096, 8192, 16);
        QVERIFY(command.error.isEmpty());
        QCOMPARE(command.threads, 16);
        QCOMPARE(command.memoryMiB, 512);
        const int index = command.arguments.indexOf("--vm-bytes");
        QVERIFY(index >= 0);
        // stress-ng applies vm-bytes to every worker, so total stays within budget.
        const int workers = command.arguments.at(command.arguments.indexOf("--vm") + 1).toInt();
        const int perWorker = command.arguments.at(index + 1).chopped(1).toInt();
        QVERIFY(perWorker * workers <= command.memoryMiB);
    }
    void invalidStressModeIsRejected()
    {
        const auto command = TestRunner::stressCommand("disk", 100, 4, 30, 512);
        QVERIFY(!command.error.isEmpty());
        QVERIFY(command.program.isEmpty());
    }
    void stressMemoryReservesOperatingSystemHeadroom()
    {
        const auto command = TestRunner::stressCommand("memory", 50, 4, 30, 4096, 1024, 8);
        QVERIFY(command.error.isEmpty());
        QCOMPARE(command.memoryMiB, 256);
        QVERIFY(!command.arguments.contains("--cpu"));
        QVERIFY(!TestRunner::stressCommand("memory", 50, 4, 30, 256, 520, 8).error.isEmpty());
        QVERIFY(!TestRunner::stressCommand("memory", 50, 4, 30, 256, -1, 8).error.isEmpty());
    }
    void benchmarkCommandUsesStableMetricAndDuration()
    {
        const auto command = TestRunner::benchmarkCommand("multi", 7, 8);
        QVERIFY(command.error.isEmpty());
        QVERIFY(command.arguments.contains("--time=7"));
        QVERIFY(command.arguments.contains("--threads=8"));
        QCOMPARE(command.unit, QStringLiteral("events/s"));
    }
    void benchmarkOutputMustContainExpectedMetric()
    {
        const auto parsed = TestRunner::parseBenchmark("single", "CPU speed:\n    events per second: 1234.50\n");
        QVERIFY(parsed.value("ok").toBool());
        QCOMPARE(parsed.value("score").toDouble(), 1234.5);
        const auto memory = TestRunner::parseBenchmark("memory", "1024.00 MiB transferred (27342.17 MiB/sec)\n");
        QVERIFY(memory.value("ok").toBool());
        QCOMPARE(memory.value("score").toDouble(), 27342.17);
        QVERIFY(!TestRunner::parseBenchmark("single", "completed").value("ok").toBool());
        QVERIFY(!TestRunner::parseBenchmark("memory", "1024.00 MiB transferred (0.00 MiB/sec)").value("ok").toBool());
        QVERIFY(!TestRunner::parseBenchmark("unknown", "events per second: 10").value("ok").toBool());
    }
    void missingToolIsRecordedWithoutScore()
    {
        TestRunner runner(nullptr, "/nonexistent/stress-ng", "/nonexistent/sysbench");
        QVERIFY(!runner.startStress("cpu", 50, 2, 5, 256));
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QCOMPARE(runner.history().size(), 1);
        QVERIFY(!runner.history().first().toMap().contains("score"));
        QVERIFY(!runner.state().value("score").isValid());
    }
    void startupIsAsynchronousAndFailureFinishesOnce()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("invalid-interpreter");
        QVERIFY(writeFile(path, "#!/no/such/interpreter\n"));
        QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        TestRunner runner(nullptr, path, path);
        QSignalSpy historyChanged(&runner, &TestRunner::historyChanged);
        QVERIFY(runner.startBenchmark("single", 1));
        // FailedToStart is processed through the event loop, not waitForStarted.
        QVERIFY(runner.state().value("running").toBool());
        QVERIFY(runner.history().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QCoreApplication::processEvents();
        QCOMPARE(historyChanged.count(), 1);
    }
    void busyRequestsDoNotChangeRunningState()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('ready', flush=True)\ntime.sleep(30)\n");
        QVERIFY(!path.isEmpty());
        TestRunner runner(nullptr, path, path);
        QVERIFY(runner.startBenchmark("single", 10));
        QTRY_VERIFY_WITH_TIMEOUT(runner.state().value("output").toString().contains("ready"), 3000);
        const auto previous = runner.state();
        QVERIFY(!runner.startStress("invalid", 1, 0, 0, 0));
        QCOMPARE(runner.state(), previous);
        QVERIFY(!runner.startBenchmark("multi", 10));
        QCOMPARE(runner.state(), previous);
        QVERIFY(runner.history().isEmpty());
        runner.stop();
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 4000);
        QCOMPARE(runner.history().first().toMap().value("status").toString(), QStringLiteral("cancelled"));
        QVERIFY(!runner.state().value("score").isValid());
    }
    void completionDrainsOutputRecordsInvocationAndLocale()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('locale=' + os.environ.get('LC_ALL', ''))\nprint('sysbench 1.0.20')\nprint('events per second: 1234.50')\n");
        TestRunner runner(nullptr, path, path);
        QVERIFY(runner.startBenchmark("single", 1));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
        const auto record = runner.history().first().toMap();
        QCOMPARE(record.value("status").toString(), QStringLiteral("completed"));
        QCOMPARE(record.value("score").toDouble(), 1234.5);
        QVERIFY(record.value("output").toString().contains("locale=C"));
        QCOMPARE(record.value("program").toString(), path);
        QVERIFY(record.value("arguments").toStringList().contains("--time=1"));
        QCOMPARE(record.value("version").toString(), QStringLiteral("1.0.20"));
    }
    void failedExitNeverExposesParsedScore()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('events per second: 100')\nsys.exit(2)\n");
        TestRunner runner(nullptr, path, path);
        QVERIFY(runner.startBenchmark("single", 1));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QVERIFY(!runner.state().value("score").isValid());
        QVERIFY(!runner.history().first().toMap().contains("score"));
    }
    void progressAdvancesWithoutOutput()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "time.sleep(30)\n");
        TestRunner runner(nullptr, path, path);
        QVERIFY(runner.startBenchmark("single", 10));
        QTRY_VERIFY_WITH_TIMEOUT(runner.state().value("elapsed").toInt() >= 1, 2500);
        QVERIFY(runner.state().value("progress").toInt() >= 10);
        runner.stop();
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 4000);
    }
    void cancellationKillsTermIgnoringDescendant()
    {
#ifdef Q_OS_LINUX
        QTemporaryDir dir;
        const auto marker = dir.filePath("child.pid");
        QByteArray body = "child = os.fork()\nif child == 0:\n    signal.signal(signal.SIGTERM, signal.SIG_IGN)\n    time.sleep(30)\n    os._exit(0)\n";
        body += "open(" + QByteArray("'") + marker.toUtf8() + "', 'w').write(str(child))\nprint('ready', flush=True)\ntime.sleep(30)\n";
        const auto path = fixture(dir, body);
        TestRunner runner(nullptr, path, path);
        QVERIFY(runner.startBenchmark("single", 10));
        QTRY_VERIFY_WITH_TIMEOUT(runner.state().value("output").toString().contains("ready"), 3000);
        QFile pidFile(marker);
        QVERIFY(pidFile.open(QIODevice::ReadOnly));
        const auto child = pidFile.readAll().toLongLong();
        const auto cleanup = qScopeGuard([child] { ::kill(static_cast<pid_t>(child), SIGKILL); });
        QVERIFY(processAlive(child));
        runner.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!processAlive(child), 4500);
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 1000);
        QCOMPARE(runner.history().first().toMap().value("status").toString(), QStringLiteral("cancelled"));
#else
        QSKIP("Linux process group test");
#endif
    }
    void actualMemoryBudgetIsCollectedBeforeLaunching()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('stress-ng version 0.17.00')\n");
        const auto proc = dir.filePath("proc");
        QVERIFY(writeFile(proc + "/meminfo", "MemAvailable: 614400 kB\n"));
        TestRunner runner(nullptr, path, path, proc);
        QVERIFY(runner.startStress("memory", 50, 2, 1, 512));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
        const auto record = runner.history().first().toMap();
        QCOMPARE(record.value("status").toString(), QStringLiteral("completed"));
        QCOMPARE(record.value("memory-mib").toInt(), 88);
        QVERIFY(record.value("arguments").toStringList().contains("44M"));
        QCOMPARE(record.value("version").toString(), QStringLiteral("0.17.00"));
    }
    void normalLeaderExitStillCleansUpDescendants()
    {
#ifdef Q_OS_LINUX
        QTemporaryDir dir;
        const auto marker = dir.filePath("child.pid");
        QByteArray body = "child = os.fork()\nif child == 0:\n    signal.signal(signal.SIGTERM, signal.SIG_IGN)\n    time.sleep(30)\n    os._exit(0)\n";
        body += "open('" + marker.toUtf8() + "', 'w').write(str(child))\nprint('events per second: 10', flush=True)\ntime.sleep(0.1)\n";
        const auto path = fixture(dir, body);
        TestRunner runner(nullptr, path, path);
        QVERIFY(runner.startBenchmark("single", 10));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(marker), 3000);
        QFile pidFile(marker);
        QVERIFY(pidFile.open(QIODevice::ReadOnly));
        const auto child = pidFile.readAll().toLongLong();
        const auto cleanup = qScopeGuard([child] { ::kill(static_cast<pid_t>(child), SIGKILL); });
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 4500);
        QTRY_VERIFY_WITH_TIMEOUT(!processAlive(child), 1000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("completed"));
#else
        QSKIP("Linux process group test");
#endif
    }
    void unavailableMemoryPreventsLaunchingAndRecordsFailure()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('must not launch')\n");
        TestRunner runner(nullptr, path, path, dir.filePath("missing-proc"));
        QVERIFY(runner.startStress("memory", 50, 2, 1, 512));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QVERIFY(runner.state().value("output").toString().isEmpty());
        QVERIFY(!runner.history().first().toMap().contains("score"));
    }
    void thermalProtectionTracksLossAndCompatibility()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('ready', flush=True)\ntime.sleep(30)\n");
        TestRunner runner(nullptr, path, path);
        QCOMPARE(runner.state().value("thermalProtection").toBool(), false);
        QCOMPARE(runner.state().value("thermalLimit").toDouble(), 90.0);
        runner.observeTemperature(true, 40.0);
        QCOMPARE(runner.state().value("thermalProtection").toBool(), true);
        QVERIFY(runner.startBenchmark("single", 10));
        QTRY_VERIFY_WITH_TIMEOUT(runner.state().value("output").toString().contains("ready"), 3000);
        runner.observeTemperature(false, 0.0);
        QCOMPARE(runner.state().value("thermalProtection").toBool(), false);
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 4000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("sensor-lost"));
        QVERIFY(!runner.state().value("score").isValid());
        runner.checkTemperature(40.0);
        QVERIFY(runner.startBenchmark("single", 10));
        QTRY_VERIFY_WITH_TIMEOUT(runner.state().value("output").toString().contains("ready"), 3000);
        runner.checkTemperature(90.0);
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 2, 4000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("thermal-stop"));
    }
    void staleTemperatureStopsProtectedRun()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "time.sleep(30)\n");
        TestRunner runner(nullptr, path, path);
        runner.observeTemperature(true, 40.0);
        QVERIFY(runner.startBenchmark("single", 10));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 5500);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("sensor-lost"));
        QCOMPARE(runner.state().value("thermalStatus").toString(), QStringLiteral("stale"));
        QCOMPARE(runner.state().value("thermalProtection").toBool(), false);
    }
    void thermalLimitRejectsBeforeProcessStarts()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('must not launch')\n");
        TestRunner runner(nullptr, path, path);
        runner.observeTemperature(true, 95.0);
        QVERIFY(!runner.startBenchmark("single", 10));
        QCOMPARE(runner.history().size(), 1);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("thermal-stop"));
        QVERIFY(runner.state().value("output").toString().isEmpty());
    }
    void expiredIdleSampleDoesNotCorruptNextStart()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('events per second: 10')\n");
        TestRunner runner(nullptr, path, path);
        runner.observeTemperature(true, 40.0);
        // No event processing: begin() itself must expire an old observation.
        QTest::qSleep(3100);
        QVERIFY(runner.startBenchmark("single", 1));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("completed"));
        QCOMPARE(runner.state().value("thermalProtection").toBool(), false);
    }
    void timeoutTerminatesSilentFixture()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "time.sleep(30)\n");
        TestRunner runner(nullptr, path, path);
        QVERIFY(runner.startStress("cpu", 50, 1, 1, 32));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 5500);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QVERIFY(!runner.history().first().toMap().contains("score"));
    }
    void stopDuringStartupAndRestartRemainIndependent()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('events per second: 10')\n");
        TestRunner runner(nullptr, path, path);
        QVERIFY(runner.startBenchmark("single", 1));
        runner.stop();
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 4000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("cancelled"));
        QVERIFY(runner.startBenchmark("single", 1));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 2, 3000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("completed"));
        QCOMPARE(runner.history().last().toMap().value("status").toString(), QStringLiteral("cancelled"));
    }
    void immediateObserverCancellationDoesNotPrepareCancelledTask()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "time.sleep(30)\n");
        const auto proc = dir.filePath("proc");
        QVERIFY(writeFile(proc + "/meminfo", "MemAvailable: 614400 kB\n"));
        TestRunner runner(nullptr, path, path, proc);
        const auto connection = connect(&runner, &TestRunner::stateChanged, &runner, [&runner] {
            if (runner.state().value("status").toString() == QStringLiteral("starting")) runner.stop();
        });
        QVERIFY(!runner.startStress("memory", 50, 2, 10, 512));
        disconnect(connection);
        QCOMPARE(runner.history().size(), 1);
        QVERIFY(runner.startBenchmark("single", 10));
        runner.stop();
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 2, 4000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("cancelled"));
    }
    void destructionKillsLeaderAndDescendant()
    {
#ifdef Q_OS_LINUX
        QTemporaryDir dir;
        const auto marker = dir.filePath("child.pid");
        QByteArray body = "child = os.fork()\nif child == 0:\n    signal.signal(signal.SIGTERM, signal.SIG_IGN)\n    time.sleep(30)\n    os._exit(0)\n";
        body += "open('" + marker.toUtf8() + "', 'w').write(str(child))\nprint('ready', flush=True)\ntime.sleep(30)\n";
        const auto path = fixture(dir, body);
        auto runner = std::make_unique<TestRunner>(nullptr, path, path);
        QVERIFY(runner->startBenchmark("single", 10));
        QTRY_VERIFY_WITH_TIMEOUT(runner->state().value("output").toString().contains("ready"), 3000);
        QFile pidFile(marker);
        QVERIFY(pidFile.open(QIODevice::ReadOnly));
        const auto child = pidFile.readAll().toLongLong();
        const auto cleanup = qScopeGuard([child] { ::kill(static_cast<pid_t>(child), SIGKILL); });
        QVERIFY(processAlive(child));
        runner.reset();
        QTRY_VERIFY_WITH_TIMEOUT(!processAlive(child), 1000);
#else
        QSKIP("Linux process group test");
#endif
    }

};

QTEST_GUILESS_MAIN(TestTestRunner)
#include "tst_testrunner.moc"
