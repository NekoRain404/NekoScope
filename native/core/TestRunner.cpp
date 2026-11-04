#include "TestRunner.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <cmath>
#include <limits>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

namespace neko {
namespace {
constexpr qsizetype kOutputLimit = 64 * 1024;
constexpr int kMemoryReserveMiB = 512;
constexpr int kMemoryHardLimitMiB = 512;
constexpr int kMaximumDurationSeconds = 3600;
constexpr int kMaximumLoadPercent = 100;
constexpr int kMinimumLoadPercent = 10;
constexpr int kKillGraceMs = 1500;
constexpr int kThermalFreshnessMs = 3000;
constexpr double kThermalLimit = 90.0;
constexpr qsizetype kHistoryLimit = 100;
constexpr auto kHistoryFileName = "benchmark-history.json";

qint64 availableMemoryMiB(const QString &procRoot)
{
    QFile file(QDir(procRoot).filePath(QStringLiteral("meminfo")));
    if (!file.open(QIODevice::ReadOnly)) return -1;
    const auto lines = file.readAll().split('\n');
    for (const auto &line : lines) {
        const auto parts = line.simplified().split(' ');
        if (parts.size() != 3 || parts.at(0) != "MemAvailable:" || parts.at(2) != "kB") continue;
        bool ok = false;
        const auto kib = parts.at(1).toLongLong(&ok);
        return ok && kib >= 0 ? kib / 1024 : -1;
    }
    return -1;
}

QString toolVersion(const QString &output)
{
    const auto match = QRegularExpression(QStringLiteral(R"((?:sysbench|stress-ng)(?:\s+version)?\s+([0-9]+(?:\.[0-9]+)+(?:[-+][\w.]+)?))"),
                                         QRegularExpression::CaseInsensitiveOption).match(output);
    return match.hasMatch() ? match.captured(1) : QString();
}
}

TestRunner::TestRunner(QObject *parent, QString stressNgPath, QString sysbenchPath, QString procRoot,
                       QString historyPath)
    : QObject(parent), m_process(this), m_preparation(this), m_killTimer(this),
      m_timeoutTimer(this), m_progressTimer(this), m_stressNgPath(std::move(stressNgPath)),
      m_sysbenchPath(std::move(sysbenchPath)), m_procRoot(std::move(procRoot)),
      m_historyPath(std::move(historyPath))
{
    if (m_historyPath.isEmpty()) {
        const auto configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        if (!configDir.isEmpty())
            m_historyPath = QDir(configDir).filePath(QString::fromLatin1(kHistoryFileName));
    }
    loadHistory();
    m_state = {{"running", false}, {"kind", QString()}, {"type", QString()},
               {"status", QStringLiteral("idle")}, {"memory-mib", 0}, {"progress", 0},
               {"elapsed", 0}, {"duration", 0}, {"load", 0}, {"threads", 0},
               {"message", QString()}, {"messageKey", QString()},
               {"messageParams", QVariantMap()}, {"reasonKey", QString()},
               {"reasonParams", QVariantMap()}, {"output", QString()}, {"score", QVariant()},
               {"unit", QString()}, {"engine", QString()}, {"thermalProtection", false},
               {"thermalLimit", kThermalLimit}, {"thermalStatus", QStringLiteral("unavailable")},
               {"thermalFreshnessMs", kThermalFreshnessMs}, {"temperature", QVariant()}};
    m_killTimer.setSingleShot(true);
    m_timeoutTimer.setSingleShot(true);
    m_progressTimer.setInterval(100);
    m_progressTimer.start();
    connect(&m_progressTimer, &QTimer::timeout, this, &TestRunner::updateProgress);
    connect(&m_killTimer, &QTimer::timeout, this, [this] {
        signalProcessGroup(SIGKILL);
        if (m_process.state() != QProcess::NotRunning) m_process.kill();
        if (m_active && m_rootFinished)
            finish(m_resultStatus, m_resultReason, m_exitCode, m_exitStatus);
    });
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this] {
        requestStop(QStringLiteral("failed"), QStringLiteral("测试超过时限，已终止"),
                    QStringLiteral("reason.timeout"));
    });
    connect(&m_preparation, &QFutureWatcher<TestCommand>::finished, this, [this] {
        m_preparing = false;
        if (!m_active) return;
        if (!m_stopStatus.isEmpty()) {
            finish(m_stopStatus, m_stopReason);
            return;
        }
        const auto command = m_preparation.result();
        if (!command.error.isEmpty())
            finish(QStringLiteral("failed"), command.error, -1, QProcess::CrashExit,
                   command.errorKey, command.errorParams);
        else launch(command);
    });
    connect(&m_process, &QProcess::started, this, [this] {
        m_processGroup = m_process.processId();
        if (!m_stopStatus.isEmpty()) {
            signalProcessGroup(SIGTERM);
            m_process.terminate();
            m_killTimer.start(kKillGraceMs);
        } else {
            m_state.insert("status", QStringLiteral("running"));
            m_state.insert("message", QStringLiteral("测试已启动"));
            m_state.insert("messageKey", QStringLiteral("message.running"));
            m_state.insert("messageParams", QVariantMap());
            m_state.insert("reasonKey", QString());
            m_state.insert("reasonParams", QVariantMap());
            emit stateChanged();
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
        QVariantMap{{"name", QStringLiteral("stress-ng")}, {"path", stress}, {"available", !stress.isEmpty()},
                    {"description", QStringLiteral("CPU 与内存压力测试")}},
        QVariantMap{{"name", QStringLiteral("sysbench")}, {"path", sysbench}, {"available", !sysbench.isEmpty()},
                    {"description", QStringLiteral("CPU 与内存基准测试")}}};
}

TestRunner::~TestRunner()
{
    m_progressTimer.stop();
    m_timeoutTimer.stop();
    m_killTimer.stop();
    disconnect(&m_preparation, nullptr, this, nullptr);
    m_preparation.waitForFinished();
    disconnect(&m_process, nullptr, this, nullptr);
    if (m_process.state() == QProcess::Starting) m_process.waitForStarted(1000);
    if (m_processGroup == 0 && m_process.state() == QProcess::Running)
        m_processGroup = m_process.processId();
    signalProcessGroup(SIGTERM);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        m_process.waitForFinished(kKillGraceMs);
    }
    // The leader may have exited while descendants ignored TERM. Keep the
    // original group ID until all cleanup, including escalation, is complete.
    signalProcessGroup(SIGKILL);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(kKillGraceMs);
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

void TestRunner::loadHistory()
{
    m_history.clear();
    if (m_historyPath.isEmpty()) return;
    QFile file(m_historyPath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) return;
    for (const auto &value : document.array()) {
        if (!value.isObject()) continue;
        const auto entry = value.toObject().toVariantMap();
        const auto kind = entry.value(QStringLiteral("kind"));
        const auto status = entry.value(QStringLiteral("status"));
        // Reject malformed roots, but keep unknown fields so older or newer records
        // round-trip without losing any key.
        if (kind.metaType().id() != QMetaType::QString || kind.toString().isEmpty()) continue;
        if (status.metaType().id() != QMetaType::QString || status.toString().isEmpty()) continue;
        m_history.append(entry);
        if (m_history.size() >= kHistoryLimit) break;
    }
}

void TestRunner::saveHistory() const
{
    if (m_historyPath.isEmpty()) return;
    QDir().mkpath(QFileInfo(m_historyPath).absolutePath());
    QJsonArray entries;
    for (const auto &entry : m_history) entries.append(QJsonValue::fromVariant(entry));
    const auto bytes = QJsonDocument(entries).toJson(QJsonDocument::Indented);
    QSaveFile file(m_historyPath);
    if (!file.open(QIODevice::WriteOnly)) return;
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return;
    }
    file.commit();
}

TestCommand TestRunner::stressCommand(const QString &mode, int load, int threads, int duration,
                                      int memoryMiB, qint64 availableMiB, int idealThreads)
{
    TestCommand result;
    if (mode != QStringLiteral("cpu") && mode != QStringLiteral("memory") &&
        mode != QStringLiteral("combined")) {
        result.error = QStringLiteral("不支持的压力测试类型");
        result.errorKey = QStringLiteral("reason.unsupported-stress-type");
        return result;
    }
    if (load < kMinimumLoadPercent || load > kMaximumLoadPercent || duration < 1 ||
        duration > kMaximumDurationSeconds || memoryMiB < 1) {
        result.error = QStringLiteral("测试参数超出允许范围");
        result.errorKey = QStringLiteral("reason.invalid-parameters");
        return result;
    }
    const int cpuCount = idealThreads > 0 ? idealThreads : qMax(1, QThread::idealThreadCount());
    const int cappedThreads = qBound(1, threads, cpuCount);
    const bool needsMemory = mode == QStringLiteral("memory") || mode == QStringLiteral("combined");
    if (needsMemory && availableMiB < 0) {
        result.error = QStringLiteral("无法读取可用内存，不能安全启动内存压力测试");
        result.errorKey = QStringLiteral("reason.memory-unavailable");
        return result;
    }
    int safeMemory = qMin(memoryMiB, kMemoryHardLimitMiB);
    if (availableMiB >= 0) {
        const qint64 reserveAdjusted = qMax<qint64>(0, availableMiB - kMemoryReserveMiB);
        safeMemory = qMin<qint64>(safeMemory,
                                  qMin<qint64>(availableMiB / 4, reserveAdjusted));
    }
    if ((mode == QStringLiteral("memory") || mode == QStringLiteral("combined")) && safeMemory < 16) {
        result.error = QStringLiteral("可用内存不足以安全启动压力测试");
        result.errorKey = QStringLiteral("reason.insufficient-memory");
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
                         << QStringLiteral("%1M").arg(safeMemory / workers)
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
        result.errorKey = QStringLiteral("reason.unsupported-benchmark-type");
        return result;
    }
    if (duration < 1 || duration > 300) {
        result.error = QStringLiteral("基准测试时长必须为 1 到 300 秒");
        result.errorKey = QStringLiteral("reason.invalid-duration");
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
    if (type != QStringLiteral("single") && type != QStringLiteral("multi") && type != QStringLiteral("memory"))
        return {{"ok", false}, {"message", QStringLiteral("不支持的基准测试类型")},
                {"messageKey", QStringLiteral("reason.unsupported-benchmark-type")},
                {"messageParams", QVariantMap()}};
    const bool isMemory = type == QStringLiteral("memory");
    const QString pattern = isMemory
                                ? QStringLiteral(R"(MiB\s+transferred\s*\(\s*([0-9]+(?:\.[0-9]+)?)\s*MiB/sec\s*\))")
                                : QStringLiteral(R"(events\s+per\s+second:\s*([0-9]+(?:\.[0-9]+)?))");
    const QRegularExpressionMatch match = QRegularExpression(pattern,
                                                               QRegularExpression::CaseInsensitiveOption)
                                             .match(output);
    if (!match.hasMatch())
        return {{"ok", false}, {"message", QStringLiteral("无法从基准输出解析有效成绩")},
                {"messageKey", QStringLiteral("reason.benchmark-unparsed")},
                {"messageParams", QVariantMap()}};
    bool ok = false;
    const double value = match.captured(1).toDouble(&ok);
    if (!ok || !std::isfinite(value) || value <= 0.0)
        return {{"ok", false}, {"message", QStringLiteral("基准成绩无效")},
                {"messageKey", QStringLiteral("reason.benchmark-invalid")},
                {"messageParams", QVariantMap()}};
    return {{"ok", true}, {"score", value},
            {"unit", isMemory ? QStringLiteral("MiB/s") : QStringLiteral("events/s")}};
}

bool TestRunner::rejectBusy()
{
    if (!m_active) return false;
    emit message(QStringLiteral("已有测试正在运行"));
    emit keyedMessage(QStringLiteral("message.busy"), QVariantMap());
    return true;
}

void TestRunner::begin(const TestCommand &command, const QString &kind, const QString &type,
                       int duration, int load)
{
    updateThermalState();
    m_active = true;
    m_rootFinished = false;
    m_processGroup = 0;
    m_kind = kind;
    m_type = type;
    m_duration = duration;
    m_startedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_elapsed.start();
    m_output.clear();
    m_version.clear();
    m_stopStatus.clear();
    m_stopReason.clear();
    m_stopReasonKey.clear();
    m_stopReasonParams.clear();
    m_resultStatus.clear();
    m_resultReason.clear();
    m_resultReasonKey.clear();
    m_resultReasonParams.clear();
    m_exitCode = -1;
    m_exitStatus = QProcess::CrashExit;
    m_state.insert("score", QVariant());
    m_state.insert("unit", command.unit);
    m_state.insert("output", QString());
    m_state.insert("engine", QFileInfo(command.program).fileName());
    m_state.insert("program", command.program);
    m_state.insert("arguments", command.arguments);
    m_state.insert("kind", kind);
    m_state.insert("type", type);
    m_state.insert("memory-mib", command.memoryMiB);
    m_state.insert("duration", duration);
    m_state.insert("load", load);
    m_state.insert("threads", command.threads);
    m_state.insert("elapsed", 0);
    m_state.insert("progress", 0);
    m_state.insert("running", true);
    m_state.insert("status", QStringLiteral("starting"));
    m_state.insert("message", QStringLiteral("正在准备测试"));
    m_state.insert("messageKey", QStringLiteral("message.preparing"));
    m_state.insert("messageParams", QVariantMap());
    m_state.insert("reasonKey", QString());
    m_state.insert("reasonParams", QVariantMap());
    emit stateChanged();
}

bool TestRunner::startStress(QString mode, int load, int threads, int duration, int memoryMiB)
{
    if (rejectBusy()) return false;
    const int ideal = qMax(1, QThread::idealThreadCount());
    auto command = stressCommand(mode, load, threads, duration, memoryMiB,
                                 std::numeric_limits<qint64>::max(), ideal);
    const auto tool = findTool(QStringLiteral("stress-ng"), m_stressNgPath);
    command.program = tool;
    begin(command, QStringLiteral("stress"), mode, duration, load);
    if (!m_active) return false;
    if (!command.error.isEmpty() || tool.isEmpty()) {
        if (!command.error.isEmpty())
            finish(QStringLiteral("failed"), command.error, -1, QProcess::CrashExit,
                   command.errorKey, command.errorParams);
        else
            finish(QStringLiteral("failed"), QStringLiteral("未找到 stress-ng；请安装后重试"),
                   -1, QProcess::CrashExit, QStringLiteral("reason.missing-tool"),
                   QVariantMap{{QStringLiteral("tool"), QStringLiteral("stress-ng")}});
        return false;
    }
    if (mode == QStringLiteral("cpu")) return launch(command);
    m_preparing = true;
    const auto procRoot = m_procRoot;
    m_preparation.setFuture(QtConcurrent::run([procRoot, mode, load, threads, duration, memoryMiB, ideal, tool] {
        auto resolved = stressCommand(mode, load, threads, duration, memoryMiB, availableMemoryMiB(procRoot), ideal);
        resolved.program = tool;
        return resolved;
    }));
    return true;
}

bool TestRunner::startBenchmark(QString type, int duration)
{
    if (rejectBusy()) return false;
    auto command = benchmarkCommand(type, duration);
    command.program = findTool(QStringLiteral("sysbench"), m_sysbenchPath);
    begin(command, QStringLiteral("benchmark"), type, duration, 100);
    if (!m_active) return false;
    if (!command.error.isEmpty() || command.program.isEmpty()) {
        if (!command.error.isEmpty())
            finish(QStringLiteral("failed"), command.error, -1, QProcess::CrashExit,
                   command.errorKey, command.errorParams);
        else
            finish(QStringLiteral("failed"), QStringLiteral("未找到 sysbench；请安装后重试"),
                   -1, QProcess::CrashExit, QStringLiteral("reason.missing-tool"),
                   QVariantMap{{QStringLiteral("tool"), QStringLiteral("sysbench")}});
        return false;
    }
    return launch(command);
}

bool TestRunner::launch(const TestCommand &command)
{
    updateThermalState();
    if (!m_active || !m_stopStatus.isEmpty()) return false;
    if (m_thermalAvailable && m_temperature >= kThermalLimit) {
        finish(QStringLiteral("thermal-stop"), QStringLiteral("温度达到 90 °C，未启动测试"),
               -1, QProcess::CrashExit, QStringLiteral("reason.thermal-stop-preflight"));
        return false;
    }
    m_state.insert("program", command.program);
    m_state.insert("arguments", command.arguments);
    m_state.insert("memory-mib", command.memoryMiB);
    m_state.insert("threads", command.threads);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    environment.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    m_process.setProcessEnvironment(environment);
#ifdef Q_OS_UNIX
    m_process.setChildProcessModifier([] {
        if (::setsid() < 0) ::_exit(126);
    });
#endif
    m_timeoutTimer.start(m_duration * 1000 + (m_kind == QStringLiteral("benchmark") ? 5000 : 2000));
    m_process.start(command.program, command.arguments, QIODevice::ReadOnly);
    return true;
}

void TestRunner::appendOutput(const QByteArray &bytes)
{
    if (!m_active || bytes.isEmpty()) return;
    m_output += QString::fromLocal8Bit(bytes);
    if (m_version.isEmpty()) m_version = toolVersion(m_output);
    if (m_output.size() > kOutputLimit) m_output = m_output.right(kOutputLimit);
    m_state.insert("output", m_output);
    emit stateChanged();
}

void TestRunner::updateProgress()
{
    updateThermalState();
    if (!m_active || !m_elapsed.isValid()) return;
    const auto elapsed = m_elapsed.elapsed() / 1000;
    const int progress = qBound(0, static_cast<int>(m_elapsed.elapsed() * 100 / qMax(1, m_duration * 1000)), 99);
    if (m_state.value("elapsed").toLongLong() == elapsed && m_state.value("progress").toInt() == progress) return;
    m_state.insert("elapsed", elapsed);
    m_state.insert("progress", progress);
    emit stateChanged();
}

void TestRunner::updateThermalState()
{
    if (m_thermalAvailable && m_thermalAge.isValid() && m_thermalAge.elapsed() > kThermalFreshnessMs) {
        m_thermalAvailable = false;
        m_state.insert("thermalStatus", QStringLiteral("stale"));
        m_state.insert("thermalProtection", false);
        m_state.insert("temperature", QVariant());
        emit stateChanged();
        if (m_active)
            requestStop(QStringLiteral("sensor-lost"), QStringLiteral("温度采集已过期，测试已停止"),
                        QStringLiteral("reason.sensor-lost-stale"));
    }
}

void TestRunner::observeTemperature(bool available, double celsius)
{
    const bool wasAvailable = m_thermalAvailable;
    m_thermalAvailable = available && std::isfinite(celsius);
    m_state.insert("thermalProtection", m_thermalAvailable);
    m_state.insert("thermalStatus", m_thermalAvailable ? QStringLiteral("fresh") : QStringLiteral("unavailable"));
    m_state.insert("temperature", m_thermalAvailable ? QVariant(celsius) : QVariant());
    if (m_thermalAvailable) {
        m_temperature = celsius;
        m_thermalAge.restart();
    }
    emit stateChanged();
    if (!m_active) return;
    if (m_thermalAvailable && celsius >= kThermalLimit)
        requestStop(QStringLiteral("thermal-stop"), QStringLiteral("温度达到 90 °C，测试已停止"),
                    QStringLiteral("reason.thermal-stop"));
    else if (wasAvailable && !m_thermalAvailable)
        requestStop(QStringLiteral("sensor-lost"), QStringLiteral("温度传感器不可用，测试已停止"),
                    QStringLiteral("reason.sensor-lost"));
}

void TestRunner::checkTemperature(double celsius) { observeTemperature(true, celsius); }
void TestRunner::stop()
{
    requestStop(QStringLiteral("cancelled"), QStringLiteral("用户取消"),
                QStringLiteral("reason.stopped-by-user"));
}

void TestRunner::requestStop(const QString &status, const QString &reason,
                             const QString &reasonKey, const QVariantMap &params)
{
    if (!m_active || !m_stopStatus.isEmpty() || m_rootFinished) return;
    m_stopStatus = status;
    m_stopReason = reason;
    m_stopReasonKey = reasonKey;
    m_stopReasonParams = params;
    m_timeoutTimer.stop();
    m_state.insert("status", QStringLiteral("stopping"));
    m_state.insert("message", reason);
    m_state.insert("messageKey", reasonKey);
    m_state.insert("messageParams", params);
    m_state.insert("reasonKey", reasonKey);
    m_state.insert("reasonParams", params);
    emit stateChanged();
    if (m_preparing) return;
    if (m_process.state() == QProcess::NotRunning) { finish(status, reason); return; }
    signalProcessGroup(SIGTERM);
    if (m_process.state() == QProcess::Running) m_process.terminate();
    m_killTimer.start(kKillGraceMs);
}

void TestRunner::signalProcessGroup(int signal)
{
#ifdef Q_OS_UNIX
    if (m_processGroup > 0) ::kill(-static_cast<pid_t>(m_processGroup), signal);
#else
    Q_UNUSED(signal);
#endif
}

bool TestRunner::groupExists() const
{
#ifdef Q_OS_UNIX
    return m_processGroup > 0 && (::kill(-static_cast<pid_t>(m_processGroup), 0) == 0 || errno == EPERM);
#else
    return false;
#endif
}

void TestRunner::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_active) return;
    appendOutput(m_process.readAllStandardOutput());
    appendOutput(m_process.readAllStandardError());
    m_rootFinished = true;
    m_exitCode = exitCode;
    m_exitStatus = exitStatus;
    m_timeoutTimer.stop();
    if (!m_stopStatus.isEmpty()) {
        m_resultStatus = m_stopStatus;
        m_resultReason = m_stopReason;
        m_resultReasonKey = m_stopReasonKey;
        m_resultReasonParams = m_stopReasonParams;
    } else if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        m_resultStatus = QStringLiteral("failed");
        m_resultReason = QStringLiteral("测试进程异常结束");
        m_resultReasonKey = QStringLiteral("reason.process-failed");
        m_resultReasonParams.clear();
    } else if (m_kind == QStringLiteral("benchmark")) {
        const auto parsed = parseBenchmark(m_type, m_output);
        const bool ok = parsed.value("ok").toBool();
        m_resultStatus = ok ? QStringLiteral("completed") : QStringLiteral("failed");
        m_resultReason = ok ? QStringLiteral("测试完成") : parsed.value("message").toString();
        m_resultReasonKey = ok ? QStringLiteral("reason.completed")
                               : parsed.value("messageKey").toString();
        m_resultReasonParams = ok ? QVariantMap() : parsed.value("messageParams").toMap();
        if (ok) {
            m_state.insert("score", parsed.value("score"));
            m_state.insert("unit", parsed.value("unit"));
        }
    } else {
        m_resultStatus = QStringLiteral("completed");
        m_resultReason = QStringLiteral("测试完成");
        m_resultReasonKey = QStringLiteral("reason.completed");
        m_resultReasonParams.clear();
    }
    if (groupExists()) {
        signalProcessGroup(SIGTERM);
        m_state.insert("status", QStringLiteral("stopping"));
        if (!m_killTimer.isActive()) m_killTimer.start(kKillGraceMs);
        emit stateChanged();
    } else finish(m_resultStatus, m_resultReason, exitCode, exitStatus);
}

void TestRunner::onError(QProcess::ProcessError error)
{
    if (m_active && error == QProcess::FailedToStart) {
        if (!m_stopStatus.isEmpty()) {
            finish(m_stopStatus, m_stopReason);
        } else {
            finish(QStringLiteral("failed"),
                   QStringLiteral("无法启动测试工具：%1").arg(m_process.errorString()),
                   -1, QProcess::CrashExit, QStringLiteral("reason.launch-failed"),
                   QVariantMap{{QStringLiteral("error"), m_process.errorString()}});
        }
    }
}

void TestRunner::finish(QString status, QString reason, int exitCode, QProcess::ExitStatus exitStatus,
                        const QString &reasonKey, const QVariantMap &params)
{
    if (!m_active) return;
    m_timeoutTimer.stop();
    m_killTimer.stop();
    m_active = false;
    m_processGroup = 0;
    QString key = reasonKey;
    QVariantMap keyParams = params;
    if (key.isEmpty()) {
        if (!m_resultReasonKey.isEmpty()) {
            key = m_resultReasonKey;
            keyParams = m_resultReasonParams;
        } else {
            key = m_stopReasonKey;
            keyParams = m_stopReasonParams;
        }
    }
    const auto endedAt = QDateTime::currentDateTimeUtc();
    const auto elapsed = m_elapsed.isValid() ? m_elapsed.elapsed() / 1000 : 0;
    m_state.insert("running", false);
    m_state.insert("status", status);
    m_state.insert("message", reason);
    m_state.insert("messageKey", key);
    m_state.insert("messageParams", keyParams);
    m_state.insert("reasonKey", key);
    m_state.insert("reasonParams", keyParams);
    m_state.insert("elapsed", elapsed);
    m_state.insert("progress", status == QStringLiteral("completed") ? QVariant(100) : m_state.value("progress"));
    m_state.insert("output", m_output);
    if (status != QStringLiteral("completed")) m_state.insert("score", QVariant());
    const auto version = m_version;
    QVariantMap record{{"kind", m_kind}, {"type", m_type}, {"status", status}, {"reason", reason},
                       {"messageKey", key}, {"messageParams", keyParams},
                       {"reasonKey", key}, {"reasonParams", keyParams},
                       {"started-at", QDateTime::fromMSecsSinceEpoch(m_startedAtMs).toUTC().toString(Qt::ISODateWithMs)},
                       {"ended-at", endedAt.toString(Qt::ISODateWithMs)}, {"elapsed", elapsed},
                       {"duration", m_duration}, {"load", m_state.value("load")}, {"threads", m_state.value("threads")},
                       {"memory-mib", m_state.value("memory-mib")}, {"engine", m_state.value("engine")},
                       {"program", m_state.value("program")}, {"arguments", m_state.value("arguments")},
                       {"version", version}, {"version-status", version.isEmpty() ? QStringLiteral("unavailable") : QStringLiteral("available")},
                       {"exit-code", m_rootFinished ? QVariant(exitCode) : QVariant()},
                       {"exit-status", !m_rootFinished ? QStringLiteral("not-started") :
                                           exitStatus == QProcess::NormalExit ? QStringLiteral("normal") : QStringLiteral("crash")},
                       {"output", m_output}, {"system", QSysInfo::prettyProductName()}, {"kernel", QSysInfo::kernelVersion()},
                       {"thermalProtection", m_state.value("thermalProtection")}, {"thermalLimit", kThermalLimit}};
    if (status == QStringLiteral("completed") && m_state.value("score").isValid()) {
        record.insert("score", m_state.value("score"));
        record.insert("unit", m_state.value("unit"));
    }
    m_history.prepend(record);
    if (m_history.size() > kHistoryLimit) m_history.removeLast();
    saveHistory();
    emit stateChanged();
    emit historyChanged();
    emit message(reason);
    emit keyedMessage(key, keyParams);
}

} // namespace neko
