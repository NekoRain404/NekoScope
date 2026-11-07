#pragma once

#include "../sensors/SensorCollector.h"

#include <QFutureWatcher>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

namespace neko {

class SensorService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList readings READ readings NOTIFY readingsChanged)
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY readingsChanged)
    Q_PROPERTY(QVariantList temperatureHistory READ temperatureHistory NOTIFY readingsChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY readingsChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int historyLimit READ historyLimit WRITE setHistoryLimit NOTIFY historyLimitChanged)

public:
    explicit SensorService(QObject *parent = nullptr);
    SensorService(QString procRoot, QString sysRoot, QObject *parent = nullptr);
    ~SensorService() override;

    QVariantList readings() const;
    QVariantList cpuHistory() const;
    QVariantList temperatureHistory() const;
    QVariantMap summary() const;
    bool paused() const;
    bool busy() const;
    int historyLimit() const { return historyLimit_; }

public slots:
    // Synchronous collection for CLI/tests. Ignored while asynchronous work is pending.
    void sample();
    // GUI scheduling entry point. Requests while busy are dropped, never queued.
    void requestSample();
    void resetStatistics();
    void setPaused(bool paused);
    void setHistoryLimit(int limit);
    Q_INVOKABLE void toggleFavorite(const QString &id);

signals:
    void readingsChanged();
    void pausedChanged();
    void busyChanged();
    void historyLimitChanged();
    void sampleFinished();
    void temperatureObserved(double celsius);
    // Emitted every completed collection, even when UI updates are paused.
    void temperatureSampled(bool available, double celsius);

private:
    struct Metric { QList<double> samples; };
    void applySnapshot(const SensorSnapshot &snapshot);
    void appendMetric(const QString &id, const QVariantMap &reading);
    void appendHistory(QVariantList &history, double value);
    void loadFavorites();

    // Declared before watcher so collector outlives watcher destruction as well.
    std::unique_ptr<SensorCollector> collector_;
    QFutureWatcher<SensorSnapshot> watcher_;
    QVariantList readings_;
    QVariantList cpuHistory_;
    QVariantList temperatureHistory_;
    QVariantMap summary_;
    bool paused_ = false;
    bool busy_ = false;
    int historyLimit_ = 90;
    QMap<QString, Metric> metrics_;
    QSet<QString> favorites_;
};

} // namespace neko
