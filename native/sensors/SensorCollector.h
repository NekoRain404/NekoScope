#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QVariantMap>
#include <chrono>

namespace neko {

struct SensorSnapshot {
    QVariantMap values;
    QVariantMap sources;
    QVariantMap groups;
    QVariantMap names;
    QVariantMap units;
    QDateTime collectedAt;
};

// Plain worker state, exclusively accessed by one collection at a time.
class SensorCollector final
{
public:
    using Clock = std::chrono::steady_clock;
    SensorCollector(QString procRoot, QString sysRoot);
    SensorSnapshot collect(Clock::time_point now = Clock::now());

private:
    struct CpuTimes { quint64 total; quint64 idle; };
    struct Counter { quint64 value; Clock::time_point at; };
    QString procRoot_;
    QString sysRoot_;
    QMap<QString, CpuTimes> previousCpuTimes_;
    QMap<QString, Counter> previousCounters_;
};

} // namespace neko
