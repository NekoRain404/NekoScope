#pragma once

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace neko {

struct TestCommand
{
    QString program;
    QStringList arguments;
    QString error;
    QString unit;
    int memoryMiB = 0;
    int threads = 0;
};

class TestRunner final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList capabilities READ capabilities CONSTANT)

public:
    explicit TestRunner(QObject *parent = nullptr, QString stressNgPath = {},
                        QString sysbenchPath = {}, QString procRoot = QStringLiteral("/proc"),
                        QString historyPath = {});
    ~TestRunner() override;

    QVariantMap state() const;
    QVariantList history() const;
    QVariantList capabilities() const;

    Q_INVOKABLE bool startStress(QString mode, int load, int threads, int duration, int memoryMiB);
    Q_INVOKABLE bool startBenchmark(QString type, int duration = 10);
    Q_INVOKABLE void stop();
    // Compatible with existing consumers; new integrations must also report loss.
    void checkTemperature(double celsius);
    void observeTemperature(bool available, double celsius);

    static TestCommand stressCommand(const QString &mode, int load, int threads, int duration,
                                     int memoryMiB, qint64 availableMiB = -1,
                                     int idealThreads = -1);
    static TestCommand benchmarkCommand(const QString &type, int duration,
                                        int idealThreads = -1);
    static QVariantMap parseBenchmark(const QString &type, const QString &output);

signals:
    void stateChanged();
    void historyChanged();
    void message(const QString &text);

private:
    bool rejectBusy();
    void begin(const TestCommand &command, const QString &kind, const QString &type,
               int duration, int load);
    bool launch(const TestCommand &command);
    void appendOutput(const QByteArray &bytes);
    void updateProgress();
    void updateThermalState();
    void requestStop(const QString &status, const QString &reason);
    void signalProcessGroup(int signal);
    bool groupExists() const;
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onError(QProcess::ProcessError error);
    void finish(QString status, QString reason, int exitCode = -1,
                QProcess::ExitStatus exitStatus = QProcess::CrashExit);
    QString findTool(const QString &name, const QString &overridePath) const;
    void loadHistory();
    void saveHistory() const;

    QProcess m_process;
    QFutureWatcher<TestCommand> m_preparation;
    QTimer m_killTimer;
    QTimer m_timeoutTimer;
    QTimer m_progressTimer;
    QElapsedTimer m_elapsed;
    QElapsedTimer m_thermalAge;
    QVariantMap m_state;
    QVariantList m_history;
    QVariantList m_capabilities;
    QString m_stressNgPath;
    QString m_sysbenchPath;
    QString m_procRoot;
    QString m_historyPath;
    QString m_kind;
    QString m_type;
    QString m_output;
    QString m_version;
    QString m_stopStatus;
    QString m_stopReason;
    QString m_resultStatus;
    QString m_resultReason;
    qint64 m_startedAtMs = 0;
    qint64 m_processGroup = 0;
    int m_duration = 0;
    bool m_active = false;
    bool m_preparing = false;
    bool m_rootFinished = false;
    bool m_thermalAvailable = false;
    double m_temperature = 0.0;
    int m_exitCode = -1;
    QProcess::ExitStatus m_exitStatus = QProcess::CrashExit;
};

} // namespace neko
