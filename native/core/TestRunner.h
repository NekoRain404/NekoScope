#pragma once

#include <QObject>
#include <QProcess>
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
                        QString sysbenchPath = {});
    ~TestRunner() override;

    QVariantMap state() const;
    QVariantList history() const;
    QVariantList capabilities() const;

    Q_INVOKABLE bool startStress(QString mode, int load, int threads, int duration, int memoryMiB);
    Q_INVOKABLE bool startBenchmark(QString type, int duration = 10);
    Q_INVOKABLE void stop();
    void checkTemperature(double celsius);

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

private slots:
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onError(QProcess::ProcessError error);

private:
    bool start(const TestCommand &command, const QString &kind, const QString &type,
               int duration, int load);
    void appendOutput(const QByteArray &bytes);
    void setStatus(const QString &status, const QString &message);
    void finish(const QString &status, const QString &reason, int exitCode = -1,
                QProcess::ExitStatus exitStatus = QProcess::CrashExit);
    void terminateProcessGroup();
    QString findTool(const QString &name, const QString &overridePath) const;

    QProcess m_process;
    QVariantMap m_state;
    QVariantList m_history;
    QVariantList m_capabilities;
    QString m_stressNgPath;
    QString m_sysbenchPath;
    QString m_kind;
    QString m_type;
    QString m_unit;
    QString m_output;
    QString m_stopStatus;
    QString m_stopReason;
    QTimer *m_killTimer = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    qint64 m_startedAtMs = 0;
    int m_duration = 0;
    int m_load = 0;
    int m_threads = 0;
    int m_memoryMiB = 0;
};

} // namespace neko
