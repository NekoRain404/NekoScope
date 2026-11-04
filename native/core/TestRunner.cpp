#include "TestRunner.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QTimer>

#ifdef Q_OS_UNIX
#include <csignal>
#include <unistd.h>
#endif

namespace neko {
namespace {
constexpr int kOutputLimit = 64 * 1024;
constexpr int kMemoryReserveMiB = 512;
constexpr int kMemoryHardLimitMiB = 512;
constexpr int kMaximumDurationSeconds = 3600;
constexpr int kMaximumLoadPercent = 100;
constexpr int kMinimumLoadPercent = 10;
constexpr int kKillGraceMs = 2000;

QStringList processArguments(const QStringList &options)
{
    return options;
}
}

TestRunner::TestRunner(QObject *parent, QString stressNgPath, QString sysbenchPath)
    : QObject(parent), m_stressNgPath(std::move(stressNgPath)),
      m_sysbenchPath(std::move(sysbenchPath)), m_killTimer(new QTimer(this)),
      m_timeoutTimer(new QTimer(this))
{
    m_state = {{"running", false}, {"kind", QString()}, {"type", QString()},
               {"status", QStringLiteral("idle")}, {"memory-mib", 0}, {"progress", 0},
               {"elapsed", 0}, {"duration", 0}, {"load", 0}, {"threads", 0},
               {"message", QString()}, {"output", QString()}, {"score", QVariant()},
               {"unit", QString()}, {"engine", QString()}};

    m_killTimer->setSingleShot(true);
    m_timeoutTimer->setSingleShot(true);
    connect(m_killTimer, &QTimer::timeout, this, [this] {
        if (m_process.state() != QProcess::NotRunning) {
            terminateProcessGroup();
            m_process.kill();
        }
    });
    connect(m_timeoutTimer, &QTimer::timeout, this, [this] {
        if (m_process.state() != QProcess::NotRunning) {
            m_stopStatus = QStringLiteral("failed");
            m_stopReason = QStringLiteral("测试超过时限，已终止");
            terminateProcessGroup();
            m_process.terminate();
            m_killTimer->start(kKillGraceMs);
        }
    });

    connect(&m_process, &QProcess::readyReadStandardOutput, this,
            [this] { appendOutput(m_process.readAllStandardOutput()); });
    connect(&m_process, &QProcess::readyReadStandardError, this,
            [this] { appendOutput(m_process.readAllStandardError()); });
    connect(&m_process, &QProcess::finished, this, &TestRunner::onFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &TestRunner::onError);

    const QString stress = findTool(QStringLiteral("stress-ng"), m_stressNgPath);
    const QString sysbench = findTool(QStringLiteral("sysbench"), m_sysbenchPath);
    m_capabilities = {
        QVariantMap{{"name", QStringLiteral("stress-ng")}, {"path", stress},
                    {"available", !stress.isEmpty()},
                    {"description", QStringLiteral("CPU 与内存压力测试")}},
        QVariantMap{{"name", QStringLiteral("sysbench")}, {"path", sysbench},
                    {"available", !sysbench.isEmpty()},
                    {"description", QStringLiteral("CPU 与内存基准测试")}}};
}

TestRunner::~TestRunner()
{
    m_timeoutTimer->stop();
    m_killTimer->stop();
    if (m_process.state() != QProcess::NotRunning) {
        terminateProcessGroup();
        m_process.terminate();
        if (!m_process.waitForFinished(kKillGraceMs)) {
            terminateProcessGroup();
            m_process.kill();
            m_process.waitForFinished(kKillGraceMs);
        }
    }
}

QVariantMap TestRunner::state() const { return m_state; }
QVariantList TestRunner::history() const { return m_history; }
QVariantList TestRunner::capabilities() const { return m_capabilities; }

QString TestRunner::findTool(const QString &name, const QString &overridePath) const
{
    if (!overridePath.isEmpty()) {
        const QFileInfo info(overridePath);
        return info.isFile() && info.isExecutable() ? info.absoluteFilePath() : QString();
    }
    return QStandardPaths::findExecutable(name);
}

TestCommand TestRunner::stressCommand(const QString &mode, int load, int threads, int duration,
                                      int memoryMiB, qint64 availableMiB, int idealThreads)
{
    TestCommand result;
    if (mode != QStringLiteral("cpu") && mode != QStringLiteral("memory") &&
        mode != QStringLiteral("combined")) {
        result.error = QStringLiteral("不支持的压力测试类型");
        return result;
    }
    if (load < kMinimumLoadPercent || load > kMaximumLoadPercent || duration < 1 ||
        duration > kMaximumDurationSeconds || memoryMiB < 1) {
        result.error = QStringLiteral("测试参数超出允许范围");
        return result;
    }
    const int cpuCount = idealThreads > 0 ? idealThreads : qMax(1, QThread::idealThreadCount());
    const int cappedThreads = qBound(1, threads, cpuCount);
    int safeMemory = qMin(memoryMiB, kMemoryHardLimitMiB);
    if (availableMiB >= 0) {
        const qint64 reserveAdjusted = qMax<qint64>(0, availableMiB - kMemoryReserveMiB);
        safeMemory = qMin<qint64>(safeMemory,
                                  qMin<qint64>(availableMiB / 4, reserveAdjusted));
    }
    if ((mode == QStringLiteral("memory") || mode == QStringLiteral("combined")) && safeMemory < 16) {
        result.error = QStringLiteral("可用内存不足以安全启动压力测试");
        return result;
    }
    result.program = QStringLiteral("stress-ng");
    result.threads = cappedThreads;
    result.memoryMiB = (mode == QStringLiteral("memory") || mode == QStringLiteral("combined"))
                           ? safeMemory : 0;
    result.arguments << QStringLiteral("--timeout") << QStringLiteral("%1s").arg(duration)
                     << QStringLiteral("--metrics-brief") << QStringLiteral("--oom-avoid");
    if (mode == QStringLiteral("cpu") || mode == QStringLiteral("combined")) {
        result.arguments << QStringLiteral("--cpu") << QString::number(cappedThreads)
                         << QStringLiteral("--cpu-load") << QString::number(load);
    }
    if (mode == QStringLiteral("memory") || mode == QStringLiteral("combined")) {
        const int workers = qMax(1, qMin(cappedThreads, safeMemory / 16));
        result.arguments << QStringLiteral("--vm") << QString::number(workers)
                         << QStringLiteral("--vm-bytes")
                         << QStringLiteral("%1M").arg(safeMemory)
                         << QStringLiteral("--vm-keep");
    }
    return result;
}

TestCommand TestRunner::benchmarkCommand(const QString &type, int duration, int idealThreads)
{
    TestCommand result;
    if (type != QStringLiteral("single") && type != QStringLiteral("multi") &&
        type != QStringLiteral("memory")) {
        result.error = QStringLiteral("不支持的基准测试类型");
        return result;
    }
    if (duration < 1 || duration > 300) {
        result.error = QStringLiteral("基准测试时长必须为 1 到 300 秒");
        return result;
    }
    const int cpuCount = idealThreads > 0 ? idealThreads : qMax(1, QThread::idealThreadCount());
    const int workers = type == QStringLiteral("single") ? 1 : cpuCount;
    result.program = QStringLiteral("sysbench");
    result.threads = workers;
    result.unit = type == QStringLiteral("memory") ? QStringLiteral("MiB/s")
                                                    : QStringLiteral("events/s");
    result.arguments << QStringLiteral("--time=%1").arg(duration)
                     << QStringLiteral("--threads=%1").arg(workers);
    if (type == QStringLiteral("memory")) {
        result.arguments << QStringLiteral("memory") << QStringLiteral("--memory-block-size=1M")
                         << QStringLiteral("--memory-total-size=1G")
                         << QStringLiteral("run");
    } else {
        result.arguments << QStringLiteral("cpu") << QStringLiteral("--cpu-max-prime=20000")
                         << QStringLiteral("run");
    }
    return result;
}

QVariantMap TestRunner::parseBenchmark(const QString &type, const QString &output)
{
    const bool isMemory = type == QStringLiteral("memory");
    const QString pattern = isMemory
                                ? QStringLiteral(R"(transferred\s*\(.*?\)\s*:\s*([0-9]+(?:\.[0-9]+)?)\s*MiB)" )
                                : QStringLiteral(R"(events\s+per\s+second:\s*([0-9]+(?:\.[0-9]+)?))");
    const QRegularExpressionMatch match = QRegularExpression(pattern,
                                                               QRegularExpression::CaseInsensitiveOption)
                                             .match(output);
    if (!match.hasMatch())
        return {{"ok", false}, {"message", QStringLiteral("无法从基准输出解析有效成绩")}};
    bool ok = false;
    const double value = match.captured(1).toDouble(&ok);
    if (!ok || !std::isfinite(value) || value <= 0.0)
        return {{"ok", false}, {"message", QStringLiteral("基准成绩无效")}};
    return {{"ok", true}, {"score", value},
            {"unit", isMemory ? QStringLiteral("MiB/s") : QStringLiteral("events/s")}};
}

bool TestRunner::startStress(QString mode, int load, int threads, int duration, int memoryMiB)
{
    const int ideal = qMax(1, QThread::idealThreadCount());
    const TestCommand command = stressCommand(mode, load, threads, duration, memoryMiB, -1, ideal);
    if (!command.error.isEmpty()) {
        setStatus(QStringLiteral("failed"), command.error);
        return false;
    }
    const QString tool = findTool(QStringLiteral("stress-ng"), m_stressNgPath);
    if (tool.isEmpty()) {
        setStatus(QStringLiteral("failed"), QStringLiteral("未找到 stress-ng；请安装后重试"));
        return false;
    }
    TestCommand resolved = command;
    resolved.program = tool;
    return start(resolved, QStringLiteral("stress"), mode, duration, load);
}

bool TestRunner::startBenchmark(QString type, int duration)
{
    const TestCommand command = benchmarkCommand(type, duration);
    if (!command.error.isEmpty()) {
        setStatus(QStringLiteral("failed"), command.error);
        return false;
    }
    const QString tool = findTool(QStringLiteral("sysbench"), m_sysbenchPath);
    if (tool.isEmpty()) {
        setStatus(QStringLiteral("failed"), QStringLiteral("未找到 sysbench；请安装后重试"));
        return false;
    }
    TestCommand resolved = command;
    resolved.program = tool;
    return start(resolved, QStringLiteral("benchmark"), type, duration, 100);
}

bool TestRunner::start(const TestCommand &command, const QString &kind, const QString &type,
                       int duration, int load)
{
    if (m_process.state() != QProcess::NotRunning) {
        setStatus(QStringLiteral("failed"), QStringLiteral("已有测试正在运行"));
        return false;
    }
    m_kind = kind;
    m_type = type;
    m_unit = command.unit;
    m_duration = duration;
    m_load = load;
    m_threads = command.threads;
    m_memoryMiB = command.memoryMiB;
    m_startedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_output.clear();
    m_stopStatus.clear();
    m_stopReason.clear();
    m_state.insert("score", QVariant());
    m_state.insert("output", QString());
    m_state.insert("engine", QFileInfo(command.program).fileName());
    m_state.insert("kind", kind);
    m_state.insert("type", type);
    m_state.insert("memory-mib", m_memoryMiB);
    m_state.insert("duration", duration);
    m_state.insert("load", load);
    m_state.insert("threads", command.threads);
    m_state.insert("elapsed", 0);
    m_state.insert("progress", 0);
    m_state.insert("running", true);
    setStatus(QStringLiteral("running"), QStringLiteral("测试已启动"));

#ifdef Q_OS_UNIX
    m_process.setChildProcessModifier([] { ::setsid(); });
#endif
    m_process.start(command.program, processArguments(command.arguments), QIODevice::ReadOnly);
    if (!m_process.waitForStarted(1000)) {
        finish(QStringLiteral("failed"), m_process.errorString());
        return false;
    }
    m_timeoutTimer->start(duration * 1000 + (kind == QStringLiteral("benchmark") ? 5000 : 2000));
    emit message(QStringLiteral("已启动 %1 %2").arg(kind, type));
    return true;
}

void TestRunner::appendOutput(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return;
    m_output += QString::fromLocal8Bit(bytes);
    if (m_output.size() > kOutputLimit)
        m_output = m_output.right(kOutputLimit);
    m_state.insert("output", m_output);
    const qint64 elapsed = qMax<qint64>(0, (QDateTime::currentMSecsSinceEpoch() - m_startedAtMs) / 1000);
    m_state.insert("elapsed", elapsed);
    m_state.insert("progress", qBound(0, static_cast<int>(elapsed * 100 / qMax(1, m_duration)), 99));
    emit stateChanged();
}

void TestRunner::stop()
{
    if (m_process.state() == QProcess::NotRunning)
        return;
    m_stopStatus = QStringLiteral("cancelled");
    m_stopReason = QStringLiteral("用户取消");
    m_timeoutTimer->stop();
    terminateProcessGroup();
    m_process.terminate();
    m_killTimer->start(kKillGraceMs);
}

void TestRunner::checkTemperature(double celsius)
{
    if (m_process.state() == QProcess::NotRunning || !std::isfinite(celsius) || celsius < 90.0)
        return;
    m_stopStatus = QStringLiteral("thermal-stop");
    m_stopReason = QStringLiteral("溫度达到 90 °C，测试已停止");
    m_timeoutTimer->stop();
    terminateProcessGroup();
    m_process.terminate();
    m_killTimer->start(kKillGraceMs);
}

void TestRunner::terminateProcessGroup()
{
#ifdef Q_OS_UNIX
    const qint64 pid = m_process.processId();
    if (pid > 0)
        ::kill(-static_cast<pid_t>(pid), SIGTERM);
#endif
}

void TestRunner::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_timeoutTimer->stop();
    m_killTimer->stop();
    if (!m_stopStatus.isEmpty()) {
        finish(m_stopStatus, m_stopReason, exitCode, exitStatus);
        return;
    }
    if (m_kind == QStringLiteral("benchmark")) {
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            finish(QStringLiteral("failed"), QStringLiteral("基准进程异常结束"), exitCode, exitStatus);
            return;
        }
        const QVariantMap parsed = parseBenchmark(m_type, m_output);
        if (!parsed.value("ok").toBool()) {
            finish(QStringLiteral("failed"), parsed.value("message").toString(), exitCode, exitStatus);
            return;
        }
        m_state.insert("score", parsed.value("score"));
        m_state.insert("unit", parsed.value("unit"));
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        finish(QStringLiteral("failed"), QStringLiteral("测试进程异常结束"), exitCode, exitStatus);
        return;
    }
    finish(QStringLiteral("completed"), QStringLiteral("测试完成"), exitCode, exitStatus);
}

void TestRunner::onError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart)
        finish(QStringLiteral("failed"), QStringLiteral("无法启动测试工具：%1").arg(m_process.errorString()));
}

void TestRunner::setStatus(const QString &status, const QString &messageText)
{
    m_state.insert("status", status);
    m_state.insert("message", messageText);
    m_state.insert("running", status == QStringLiteral("running"));
    emit stateChanged();
    emit message(messageText);
}

void TestRunner::finish(const QString &status, const QString &reason, int exitCode,
                        QProcess::ExitStatus exitStatus)
{
    m_timeoutTimer->stop();
    m_killTimer->stop();
    const qint64 endedAt = QDateTime::currentMSecsSinceEpoch();
    const int elapsed = m_startedAtMs > 0 ? qMax<qint64>(0, (endedAt - m_startedAtMs) / 1000) : 0;
    m_state.insert("running", false);
    m_state.insert("status", status);
    m_state.insert("message", reason);
    m_state.insert("elapsed", elapsed);
    m_state.insert("progress", status == QStringLiteral("completed") ? 100 : m_state.value("progress"));
    m_state.insert("output", m_output);

    QVariantMap record{{"kind", m_kind}, {"type", m_type}, {"status", status},
                       {"reason", reason}, {"started-at", m_startedAtMs > 0
                                                               ? QDateTime::fromMSecsSinceEpoch(m_startedAtMs).toString(Qt::ISODate)
                                                               : QString()},
                       {"ended-at", QDateTime::fromMSecsSinceEpoch(endedAt).toString(Qt::ISODate)},
                       {"elapsed", elapsed}, {"duration", m_duration}, {"load", m_load},
                       {"threads", m_threads}, {"memory-mib", m_memoryMiB},
                       {"engine", m_state.value("engine")}, {"exit-code", exitCode},
                       {"exit-status", exitStatus == QProcess::NormalExit ? QStringLiteral("normal")
                                                                           : QStringLiteral("crash")},
                       {"output", m_output}, {"system", QSysInfo::prettyProductName()},
                       {"kernel", QSysInfo::kernelVersion()}};
    if (status == QStringLiteral("completed") && m_state.value("score").isValid()) {
        record.insert("score", m_state.value("score"));
        record.insert("unit", m_state.value("unit"));
    }
    m_history.prepend(record);
    if (m_history.size() > 100)
        m_history.removeLast();
    m_startedAtMs = 0;
    emit stateChanged();
    emit historyChanged();
    emit message(reason);
}

} // namespace neko
