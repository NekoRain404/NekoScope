#include "../core/TestRunner.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
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
QString historyPath(const QTemporaryDir &dir)
{
    return dir.filePath(QStringLiteral("benchmark-history.json"));
}
QString fixture(QTemporaryDir &dir, const QByteArray &body, const QString &name = QStringLiteral("fixture"))
{
    const auto path = dir.filePath(name);
    if (!writeFile(path, QByteArray("#!/usr/bin/python3\nimport os, sys, time, signal\n") + body)) return {};
    if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) return {};
    return path;
}
bool asciiKey(const QString &value)
{
    if (value.isEmpty()) return false;
    for (const QChar character : value)
        if (character.unicode() > 0x7f) return false;
    return true;
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
        QTemporaryDir dir;
        TestRunner runner(nullptr, "/nonexistent/stress-ng", "/nonexistent/sysbench",
                          QStringLiteral("/proc"), historyPath(dir));
        QVERIFY(!runner.startStress("cpu", 50, 2, 5, 256));
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QCOMPARE(runner.history().size(), 1);
        QVERIFY(!runner.history().first().toMap().contains("score"));
        QCOMPARE(runner.history().first().toMap().value("exit-status").toString(), QStringLiteral("not-started"));
        QVERIFY(!runner.history().first().toMap().value("exit-code").isValid());
        QVERIFY(!runner.state().value("score").isValid());
        const auto record = runner.history().first().toMap();
        QVERIFY(asciiKey(record.value("reasonKey").toString()));
        QCOMPARE(record.value("reasonKey").toString(), QStringLiteral("reason.missing-tool"));
        QCOMPARE(record.value("reasonParams").toMap().value("tool").toString(), QStringLiteral("stress-ng"));
        QVERIFY(asciiKey(runner.state().value("messageKey").toString()));
    }
    void startupIsAsynchronousAndFailureFinishesOnce()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("invalid-interpreter");
        QVERIFY(writeFile(path, "#!/no/such/interpreter\n"));
        QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
        QSignalSpy historyChanged(&runner, &TestRunner::historyChanged);
        QVERIFY(runner.startBenchmark("single", 1));
        // FailedToStart is processed through the event loop, not waitForStarted.
        QVERIFY(runner.state().value("running").toBool());
        QVERIFY(runner.history().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QCoreApplication::processEvents();
        QCOMPARE(historyChanged.count(), 1);
        QCOMPARE(runner.history().first().toMap().value("exit-status").toString(), QStringLiteral("not-started"));
    }
    void busyRequestsDoNotChangeRunningState()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('ready', flush=True)\ntime.sleep(30)\n");
        QVERIFY(!path.isEmpty());
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
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
        QCOMPARE(runner.history().first().toMap().value("reasonKey").toString(),
                 QStringLiteral("reason.stopped-by-user"));
        QVERIFY(!runner.state().value("score").isValid());
    }
    void completionDrainsOutputRecordsInvocationAndLocale()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('locale=' + os.environ.get('LC_ALL', ''))\nprint('sysbench 1.0.20')\nprint('events per second: 1234.50')\n");
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
        QVERIFY(runner.startBenchmark("single", 1));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
        const auto record = runner.history().first().toMap();
        QCOMPARE(record.value("status").toString(), QStringLiteral("completed"));
        QCOMPARE(record.value("score").toDouble(), 1234.5);
        QVERIFY(record.value("output").toString().contains("locale=C"));
        QCOMPARE(record.value("program").toString(), path);
        QVERIFY(record.value("arguments").toStringList().contains("--time=1"));
        QCOMPARE(record.value("version").toString(), QStringLiteral("1.0.20"));
        QCOMPARE(record.value("reasonKey").toString(), QStringLiteral("reason.completed"));
        QVERIFY(asciiKey(record.value("messageKey").toString()));
    }
    void failedExitNeverExposesParsedScore()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('events per second: 100')\nsys.exit(2)\n");
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
        QVERIFY(runner.startBenchmark("single", 1));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QVERIFY(!runner.state().value("score").isValid());
        QVERIFY(!runner.history().first().toMap().contains("score"));
        const auto record = runner.history().first().toMap();
        QVERIFY(asciiKey(record.value("reasonKey").toString()));
        QVERIFY(!record.value("reason").toString().isEmpty());
        QVERIFY(asciiKey(runner.state().value("messageKey").toString()));
    }
    void progressAdvancesWithoutOutput()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "time.sleep(30)\n");
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
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
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
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
        TestRunner runner(nullptr, path, path, proc, historyPath(dir));
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
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
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
        TestRunner runner(nullptr, path, path, dir.filePath("missing-proc"), historyPath(dir));
        QVERIFY(runner.startStress("memory", 50, 2, 1, 512));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QVERIFY(runner.state().value("output").toString().isEmpty());
        QVERIFY(!runner.history().first().toMap().contains("score"));
        QCOMPARE(runner.history().first().toMap().value("reasonKey").toString(),
                 QStringLiteral("reason.memory-unavailable"));
    }
    void thermalProtectionTracksLossAndCompatibility()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('ready', flush=True)\ntime.sleep(30)\n");
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
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
        QCOMPARE(runner.history().first().toMap().value("reasonKey").toString(),
                 QStringLiteral("reason.sensor-lost"));
        QVERIFY(!runner.state().value("score").isValid());
        runner.checkTemperature(40.0);
        QVERIFY(runner.startBenchmark("single", 10));
        QTRY_VERIFY_WITH_TIMEOUT(runner.state().value("output").toString().contains("ready"), 3000);
        runner.checkTemperature(90.0);
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 2, 4000);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("thermal-stop"));
        QCOMPARE(runner.history().first().toMap().value("reasonKey").toString(),
                 QStringLiteral("reason.thermal-stop"));
    }
    void staleTemperatureStopsProtectedRun()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "time.sleep(30)\n");
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
        runner.observeTemperature(true, 40.0);
        QVERIFY(runner.startBenchmark("single", 10));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 5500);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("sensor-lost"));
        QCOMPARE(runner.state().value("thermalStatus").toString(), QStringLiteral("stale"));
        QCOMPARE(runner.state().value("thermalProtection").toBool(), false);
        QCOMPARE(runner.history().first().toMap().value("reasonKey").toString(),
                 QStringLiteral("reason.sensor-lost-stale"));
    }
    void thermalLimitRejectsBeforeProcessStarts()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('must not launch')\n");
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
        runner.observeTemperature(true, 95.0);
        QVERIFY(!runner.startBenchmark("single", 10));
        QCOMPARE(runner.history().size(), 1);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("thermal-stop"));
        QVERIFY(runner.state().value("output").toString().isEmpty());
        QCOMPARE(runner.history().first().toMap().value("reasonKey").toString(),
                 QStringLiteral("reason.thermal-stop-preflight"));
    }
    void expiredIdleSampleDoesNotCorruptNextStart()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('events per second: 10')\n");
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
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
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
        QVERIFY(runner.startStress("cpu", 50, 1, 1, 32));
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 5500);
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QVERIFY(!runner.history().first().toMap().contains("score"));
        QCOMPARE(runner.history().first().toMap().value("reasonKey").toString(),
                 QStringLiteral("reason.timeout"));
    }
    void stopDuringStartupAndRestartRemainIndependent()
    {
        QTemporaryDir dir;
        const auto path = fixture(dir, "print('events per second: 10')\n");
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
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
        TestRunner runner(nullptr, path, path, proc, historyPath(dir));
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
        auto runner = std::make_unique<TestRunner>(nullptr, path, path, QStringLiteral("/proc"), historyPath(dir));
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

    void historyPersistsCompletedAttempt()
    {
        QTemporaryDir dir;
        const auto history = historyPath(dir);
        const auto path = fixture(dir, "print('sysbench 1.0.20')\nprint('events per second: 999.50')\n");
        QVERIFY(!path.isEmpty());
        {
            TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), history);
            QVERIFY(runner.startBenchmark("single", 1));
            QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 3000);
            QCOMPARE(runner.history().first().toMap().value("status").toString(),
                     QStringLiteral("completed"));
        }
        QVERIFY(QFileInfo::exists(history));
        TestRunner restored(nullptr, path, path, QStringLiteral("/proc"), history);
        QCOMPARE(restored.history().size(), 1);
        const auto record = restored.history().first().toMap();
        QCOMPARE(record.value("status").toString(), QStringLiteral("completed"));
        QCOMPARE(record.value("score").toDouble(), 999.5);
        QCOMPARE(record.value("unit").toString(), QStringLiteral("events/s"));
        QCOMPARE(record.value("program").toString(), path);
        QVERIFY(record.value("arguments").toStringList().contains("--time=1"));
        QCOMPARE(record.value("exit-status").toString(), QStringLiteral("normal"));
    }
    void historyPersistsFailedAttempt()
    {
        QTemporaryDir dir;
        const auto history = historyPath(dir);
        {
            TestRunner runner(nullptr, "/nonexistent/stress-ng", "/nonexistent/sysbench",
                              QStringLiteral("/proc"), history);
            QVERIFY(!runner.startStress("cpu", 50, 1, 1, 32));
            QCOMPARE(runner.history().size(), 1);
            QCOMPARE(runner.history().first().toMap().value("status").toString(),
                     QStringLiteral("failed"));
        }
        TestRunner restored(nullptr, "/nonexistent/stress-ng", "/nonexistent/sysbench",
                            QStringLiteral("/proc"), history);
        QCOMPARE(restored.history().size(), 1);
        QCOMPARE(restored.history().first().toMap().value("status").toString(),
                 QStringLiteral("failed"));
        QVERIFY(!restored.history().first().toMap().contains("score"));
    }
    void missingHistoryFileIsIgnoredAndCreatedOnFinish()
    {
        QTemporaryDir dir;
        const auto history = dir.filePath("nested/benchmark-history.json");
        {
            TestRunner runner(nullptr, "/nonexistent/stress-ng", "/nonexistent/sysbench",
                              QStringLiteral("/proc"), history);
            QVERIFY(runner.history().isEmpty());
            QVERIFY(!runner.startStress("cpu", 50, 1, 1, 32));
        }
        QVERIFY(QFileInfo::exists(history));
        TestRunner restored(nullptr, "/nonexistent/stress-ng", "/nonexistent/sysbench",
                            QStringLiteral("/proc"), history);
        QCOMPARE(restored.history().size(), 1);
        QCOMPARE(restored.history().first().toMap().value("status").toString(),
                 QStringLiteral("failed"));
    }
    void corruptHistoryFileIsIgnored()
    {
        QTemporaryDir dir;
        const auto history = historyPath(dir);
        QVERIFY(writeFile(history, "{ this is not valid json"));
        TestRunner runner(nullptr, "", "", QStringLiteral("/proc"), history);
        QVERIFY(runner.history().isEmpty());
    }
    void wrongHistoryRootTypeIsIgnored()
    {
        QTemporaryDir dir;
        const auto history = historyPath(dir);
        QVERIFY(writeFile(history, R"({"entries":[{"kind":"benchmark","status":"completed"}]})"));
        {
            TestRunner runner(nullptr, "", "", QStringLiteral("/proc"), history);
            QVERIFY(runner.history().isEmpty());
        }
        QVERIFY(writeFile(history, R"("not an array")"));
        {
            TestRunner runner(nullptr, "", "", QStringLiteral("/proc"), history);
            QVERIFY(runner.history().isEmpty());
        }
    }
    void historyLoadDropsInvalidEntriesAndPreservesUnknownFields()
    {
        QTemporaryDir dir;
        const auto history = historyPath(dir);
        const QVariantList entries{
            QVariantMap{{"kind", "benchmark"}, {"status", "completed"},
                        {"custom-note", "keep"}, {"extra", QVariantMap{{"nested", 42}}}},
            QVariant(42),
            QVariantMap{{"kind", "benchmark"}},
            QVariantMap{{"status", "completed"}},
            QVariantMap{{"kind", 7}, {"status", "completed"}},
            QVariantMap{{"kind", "benchmark"}, {"status", ""}}};
        QVERIFY(writeFile(history, QJsonDocument(QJsonArray::fromVariantList(entries)).toJson()));
        TestRunner runner(nullptr, "", "", QStringLiteral("/proc"), history);
        QCOMPARE(runner.history().size(), 1);
        const auto record = runner.history().first().toMap();
        QCOMPARE(record.value("custom-note").toString(), QStringLiteral("keep"));
        QCOMPARE(record.value("extra").toMap().value("nested").toInt(), 42);
    }
    void historyLoadIsBoundedToNewestHundred()
    {
        QTemporaryDir dir;
        const auto history = historyPath(dir);
        QVariantList entries;
        for (int i = 0; i < 150; ++i)
            entries.append(QVariantMap{{"kind", "benchmark"}, {"status", "completed"}, {"index", i}});
        QVERIFY(writeFile(history, QJsonDocument(QJsonArray::fromVariantList(entries)).toJson()));
        TestRunner runner(nullptr, "", "", QStringLiteral("/proc"), history);
        QCOMPARE(runner.history().size(), 100);
        QCOMPARE(runner.history().first().toMap().value("index").toInt(), 0);
        QCOMPARE(runner.history().last().toMap().value("index").toInt(), 99);
    }
    void finishedAttemptKeepsHistoryBounded()
    {
        QTemporaryDir dir;
        const auto history = historyPath(dir);
        QVariantList entries;
        for (int i = 0; i < 100; ++i)
            entries.append(QVariantMap{{"kind", "benchmark"}, {"status", "completed"}, {"index", i}});
        QVERIFY(writeFile(history, QJsonDocument(QJsonArray::fromVariantList(entries)).toJson()));
        const auto path = fixture(dir, "print('events per second: 10')\n");
        QVERIFY(!path.isEmpty());
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), history);
        QCOMPARE(runner.history().size(), 100);
        QVERIFY(runner.startBenchmark("single", 1));
        // Loaded entries are already completed, so wait for the new record instead.
        QTRY_VERIFY_WITH_TIMEOUT(runner.history().first().toMap().contains("started-at"), 3000);
        QCOMPARE(runner.history().size(), 100);
        QCOMPARE(runner.history().first().toMap().value("status").toString(),
                 QStringLiteral("completed"));
        QCOMPARE(runner.history().last().toMap().value("index").toInt(), 98);
    }
    void inProgressAttemptIsNotPersisted()
    {
        QTemporaryDir dir;
        const auto history = historyPath(dir);
        const auto path = fixture(dir, "print('ready', flush=True)\ntime.sleep(30)\n");
        QVERIFY(!path.isEmpty());
        TestRunner runner(nullptr, path, path, QStringLiteral("/proc"), history);
        QVERIFY(runner.startBenchmark("single", 10));
        QTRY_VERIFY_WITH_TIMEOUT(runner.state().value("output").toString().contains("ready"), 3000);
        QVERIFY(runner.history().isEmpty());
        QVERIFY(!QFileInfo::exists(history));
        runner.stop();
        QTRY_COMPARE_WITH_TIMEOUT(runner.history().size(), 1, 4000);
        QVERIFY(QFileInfo::exists(history));
        QCOMPARE(runner.history().first().toMap().value("status").toString(),
                 QStringLiteral("cancelled"));
    }

};

QTEST_GUILESS_MAIN(TestTestRunner)
#include "tst_testrunner.moc"
