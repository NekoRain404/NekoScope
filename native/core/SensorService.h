#pragma once

#include <QObject>
#include <QMap>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

namespace neko {

class SensorService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList readings READ readings NOTIFY readingsChanged)
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY readingsChanged)
    Q_PROPERTY(QVariantList temperatureHistory READ temperatureHistory NOTIFY readingsChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY readingsChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)

public:
    explicit SensorService(QObject *parent = nullptr);
    SensorService(QString procRoot, QString sysRoot, QObject *parent = nullptr);

    QVariantList readings() const;
    QVariantList cpuHistory() const;
    QVariantList temperatureHistory() const;
    QVariantMap summary() const;
    bool paused() const;

public slots:
    void sample();
    void resetStatistics();
    void setPaused(bool paused);
    Q_INVOKABLE void toggleFavorite(const QString &id);

signals:
    void readingsChanged();
    void pausedChanged();
    void temperatureObserved(double celsius);

private:
    struct Metric {
        QString name;
        QString group;
        QString unit;
        QString source;
        double current = 0.0;
        double minimum = 0.0;
        double maximum = 0.0;
        double total = 0.0;
        quint64 count = 0;
    };
    void collect(QVariantMap &values, QVariantMap &sources, QVariantMap &groups,
                 QVariantMap &names, QVariantMap &units, QVariantMap &mins, QVariantMap &maxs);
    void appendMetric(const QString &id, const QString &name, const QString &group,
                      double value, const QString &unit, const QString &source,
                      double minimum = 0.0, double maximum = 0.0);
    void loadFavorites();

    QString procRoot_;
    QString sysRoot_;
    QVariantList readings_;
    QVariantList cpuHistory_;
    QVariantList temperatureHistory_;
    QVariantMap summary_;
    bool paused_ = false;
    QMap<QString, Metric> metrics_;
    QMap<QString, quint64> previousCounters_;
    QMap<QString, quint64> previousCpuTimes_;
    QSet<QString> favorites_;
    double lastCpuUsage_ = -1.0;
    double lastMemoryUsage_ = -1.0;
    double lastMemoryUsedGiB_ = 0.0;
    double lastMemoryTotalGiB_ = 0.0;
    double lastCpuTemperature_ = -1.0;
    double lastCpuFrequencyGHz_ = -1.0;
    double lastCpuPower_ = -1.0;
};

} // namespace neko
