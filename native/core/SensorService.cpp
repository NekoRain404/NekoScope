#include "SensorService.h"

#include <QSettings>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <numeric>

namespace neko {
namespace {
QVariantMap emptySummary()
{
    return {{QStringLiteral("cpuUsage"), -1.0}, {QStringLiteral("memoryUsage"), -1.0},
            {QStringLiteral("memoryUsedGiB"), 0.0}, {QStringLiteral("memoryTotalGiB"), 0.0},
            {QStringLiteral("cpuTemperature"), -1.0}, {QStringLiteral("cpuFrequencyGHz"), -1.0},
            {QStringLiteral("cpuPower"), -1.0}, {QStringLiteral("sensorCount"), 0}};
}
}

SensorService::SensorService(QObject *parent)
    : SensorService(QStringLiteral("/proc"), QStringLiteral("/sys"), parent)
{
}

SensorService::SensorService(QString procRoot, QString sysRoot, QObject *parent)
    : QObject(parent), collector_(std::make_unique<SensorCollector>(std::move(procRoot), std::move(sysRoot))), watcher_(this), summary_(emptySummary())
{
    loadFavorites();
    connect(&watcher_, &QFutureWatcher<SensorSnapshot>::finished, this, [this] {
        applySnapshot(watcher_.result());
        busy_ = false;
        emit busyChanged();
        emit sampleFinished();
    });
}

SensorService::~SensorService()
{
    // Workers only capture collector_, never this or any QObject. Await the last
    // read before releasing that state; no queued callback can run after close.
    disconnect(&watcher_, nullptr, this, nullptr);
    watcher_.waitForFinished();
}

QVariantList SensorService::readings() const { return readings_; }
QVariantList SensorService::cpuHistory() const { return cpuHistory_; }
QVariantList SensorService::temperatureHistory() const { return temperatureHistory_; }
QVariantMap SensorService::summary() const { return summary_; }
bool SensorService::paused() const { return paused_; }
bool SensorService::busy() const { return busy_; }

void SensorService::sample()
{
    if (busy_) return;
    applySnapshot(collector_->collect());
    emit sampleFinished();
}

void SensorService::requestSample()
{
    if (busy_) return;
    busy_ = true;
    emit busyChanged();
    auto *collector = collector_.get();
    watcher_.setFuture(QtConcurrent::run([collector] { return collector->collect(); }));
}

void SensorService::setPaused(bool paused)
{
    if (paused_ == paused) return;
    paused_ = paused;
    emit pausedChanged();
}

void SensorService::toggleFavorite(const QString &id)
{
    if (favorites_.contains(id)) favorites_.remove(id); else favorites_.insert(id);
    QSettings settings;
    settings.setValue(QStringLiteral("neko/sensorFavorites"), QStringList(favorites_.begin(), favorites_.end()));
    for (auto &entry : readings_) {
        auto item = entry.toMap();
        if (item.value(QStringLiteral("id")).toString() == id)
            item.insert(QStringLiteral("favorite"), favorites_.contains(id));
        entry = item;
    }
    emit readingsChanged();
}

void SensorService::loadFavorites()
{
    QSettings settings;
    const auto stored = settings.value(QStringLiteral("neko/sensorFavorites")).toStringList();
    favorites_ = QSet<QString>(stored.begin(), stored.end());
}

void SensorService::appendMetric(const QString &id, const QVariantMap &reading)
{
    auto &samples = metrics_[id].samples;
    samples.append(reading.value(QStringLiteral("value")).toDouble());
    while (samples.size() > historyLimit_) samples.removeFirst();
    auto item = reading;
    item.insert(QStringLiteral("min"), *std::min_element(samples.cbegin(), samples.cend()));
    item.insert(QStringLiteral("max"), *std::max_element(samples.cbegin(), samples.cend()));
    item.insert(QStringLiteral("avg"), std::accumulate(samples.cbegin(), samples.cend(), 0.0) / static_cast<double>(samples.size()));
    item.insert(QStringLiteral("favorite"), favorites_.contains(id));
    readings_.append(item);
}

void SensorService::applySnapshot(const SensorSnapshot &snapshot)
{
    const auto &values = snapshot.values;
    const bool temperatureAvailable = values.contains(QStringLiteral("cpu.temperature"));
    const double cpuTemp = values.value(QStringLiteral("cpu.temperature"), -1.0).toDouble();
    emit temperatureSampled(temperatureAvailable, cpuTemp);
    if (temperatureAvailable) emit temperatureObserved(cpuTemp);
    // Even a paused display forgets absent channels' statistics. On resume a
    // reappearing device starts a new window instead of inheriting stale data.
    for (auto it = metrics_.begin(); it != metrics_.end();) {
        if (!values.contains(it.key())) it = metrics_.erase(it); else ++it;
    }
    if (paused_) return;

    readings_.clear();
    const auto timestamp = snapshot.collectedAt.toString(Qt::ISODateWithMs);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const auto &id = it.key();
        appendMetric(id, {{QStringLiteral("id"), id}, {QStringLiteral("name"), snapshot.names.value(id)},
                         {QStringLiteral("group"), snapshot.groups.value(id)}, {QStringLiteral("value"), it.value()},
                         {QStringLiteral("unit"), snapshot.units.value(id)}, {QStringLiteral("source"), snapshot.sources.value(id)},
                         {QStringLiteral("deviceId"), snapshot.deviceIds.value(id)}, {QStringLiteral("device"), snapshot.deviceNames.value(id)},
                         {QStringLiteral("status"), QStringLiteral("available")}, {QStringLiteral("updatedAt"), timestamp}});
    }
    if (values.contains(QStringLiteral("cpu.usage"))) appendHistory(cpuHistory_, values.value(QStringLiteral("cpu.usage")).toDouble());
    if (temperatureAvailable) appendHistory(temperatureHistory_, cpuTemp);
    summary_ = emptySummary();
    const std::pair<QString, QString> fields[] = {
        {QStringLiteral("cpuUsage"), QStringLiteral("cpu.usage")}, {QStringLiteral("memoryUsage"), QStringLiteral("memory.usage")},
        {QStringLiteral("memoryUsedGiB"), QStringLiteral("memory.used")}, {QStringLiteral("memoryTotalGiB"), QStringLiteral("memory.total")},
        {QStringLiteral("cpuTemperature"), QStringLiteral("cpu.temperature")}, {QStringLiteral("cpuFrequencyGHz"), QStringLiteral("cpu.frequency")},
        {QStringLiteral("cpuPower"), QStringLiteral("cpu.power")}};
    for (const auto &[key, id] : fields) if (values.contains(id)) summary_.insert(key, values.value(id));
    summary_.insert(QStringLiteral("sensorCount"), readings_.size());
    summary_.insert(QStringLiteral("updatedAt"), timestamp);
    emit readingsChanged();
}

void SensorService::appendHistory(QVariantList &history, double value)
{
    history.append(value);
    while (history.size() > historyLimit_) history.removeFirst();
}

void SensorService::setHistoryLimit(int limit)
{
    limit = std::clamp(limit, 10, 600);
    if (historyLimit_ == limit) return;
    historyLimit_ = limit;
    for (auto &metric : metrics_) {
        while (metric.samples.size() > historyLimit_) metric.samples.removeFirst();
    }
    while (cpuHistory_.size() > historyLimit_) cpuHistory_.removeFirst();
    while (temperatureHistory_.size() > historyLimit_) temperatureHistory_.removeFirst();
    emit historyLimitChanged();
}

void SensorService::resetStatistics()
{
    metrics_.clear();
    cpuHistory_.clear();
    temperatureHistory_.clear();
    const auto current = readings_;
    readings_.clear();
    for (const auto &entry : current) {
        const auto item = entry.toMap();
        appendMetric(item.value(QStringLiteral("id")).toString(), item);
    }
    emit readingsChanged();
}

} // namespace neko
