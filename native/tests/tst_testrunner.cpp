#include "../core/TestRunner.h"

#include <QtTest>

using neko::TestCommand;
using neko::TestRunner;

class TestTestRunner final : public QObject
{
    Q_OBJECT

private slots:
    void stressCommandsAreBounded()
    {
        const auto command = TestRunner::stressCommand("combined", 80, 16, 3600, 4096);
        QVERIFY(command.error.isEmpty());
        QCOMPARE(command.program, QStringLiteral("stress-ng"));
        QVERIFY(command.arguments.contains(QStringLiteral("--cpu")));
        QVERIFY(command.arguments.contains(QStringLiteral("--vm")));
        QVERIFY(command.arguments.contains(QStringLiteral("--timeout")));
        QVERIFY(command.arguments.contains(QStringLiteral("--oom-avoid")));
        QCOMPARE(command.threads, 16);
        QVERIFY(command.memoryMiB <= 512);
    }

    void invalidStressModeIsRejected()
    {
        const auto command = TestRunner::stressCommand("disk", 100, 4, 30, 512);
        QVERIFY(!command.error.isEmpty());
        QVERIFY(command.program.isEmpty());
        QVERIFY(command.arguments.isEmpty());
    }

    void stressMemoryReservesOperatingSystemHeadroom()
    {
        const auto command = TestRunner::stressCommand("memory", 50, 4, 30, 4096, 1024, 8);
        QVERIFY(command.error.isEmpty());
        QCOMPARE(command.memoryMiB, 512);
        QVERIFY(!command.arguments.contains(QStringLiteral("--cpu")));
    }

    void benchmarkCommandUsesStableMetricAndDuration()
    {
        const auto command = TestRunner::benchmarkCommand("multi", 7, 8);
        QVERIFY(command.error.isEmpty());
        QCOMPARE(command.program, QStringLiteral("sysbench"));
        QVERIFY(command.arguments.contains(QStringLiteral("--time=7")));
        QVERIFY(command.arguments.contains(QStringLiteral("--threads=8")));
        QCOMPARE(command.unit, QStringLiteral("events/s"));
    }

    void benchmarkOutputMustContainExpectedMetric()
    {
        const auto parsed = TestRunner::parseBenchmark(
            "single", QStringLiteral("CPU speed:\n    events per second: 1234.50\n"));
        QVERIFY(parsed.value("ok").toBool());
        QCOMPARE(parsed.value("score").toDouble(), 1234.50);
        QCOMPARE(parsed.value("unit").toString(), QStringLiteral("events/s"));

        const auto malformed = TestRunner::parseBenchmark("single", QStringLiteral("completed"));
        QVERIFY(!malformed.value("ok").toBool());
    }

    void missingToolFailsWithoutStartingAProcess()
    {
        TestRunner runner(nullptr, QStringLiteral("/nonexistent/stress-ng"),
                          QStringLiteral("/nonexistent/sysbench"));
        QVERIFY(!runner.startStress(QStringLiteral("cpu"), 50, 2, 5, 256));
        QCOMPARE(runner.state().value("status").toString(), QStringLiteral("failed"));
        QVERIFY(!runner.state().value("message").toString().isEmpty());
        QVERIFY(runner.history().size() == 1);
    }
};

QTEST_MAIN(TestTestRunner)
#include "tst_testrunner.moc"
