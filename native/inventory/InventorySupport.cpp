#include "InventoryContext.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTextStream>
#include <QTime>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <limits>


namespace neko::inventory {

ReadResult Collector::readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QFileInfo info(path);
        return {{}, !info.exists() ? Unavailable : file.error() == QFileDevice::PermissionsError ? PermissionDenied : ReadError};
    }
    QTextStream stream(&file);
    const QString value = stream.readAll().trimmed();
    if (file.error() != QFileDevice::NoError) return {{}, ReadError};
    return value.isEmpty() ? ReadResult{{}, Unavailable} : ReadResult{value, Available};
}

QString Collector::cleanId(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.:-]+")), QStringLiteral("_"));
    return value;
}

QVariantMap Collector::field(const QString &key, const ReadResult &result, const QString &source,
                  bool sensitive)
{
    return {{QStringLiteral("key"), key},
            {QStringLiteral("value"), result.value},
            {QStringLiteral("source"), source},
            {QStringLiteral("status"), result.status},
            {QStringLiteral("sensitive"), sensitive}};
}

QVariantMap Collector::textField(const QString &key, const QString &path, bool sensitive)
{
    return field(key, readText(path), path, sensitive);
}

QVariantMap Collector::valueField(const QString &key, const QString &value, const QString &source,
                       bool sensitive)
{
    return field(key, value.isEmpty() ? ReadResult{} : ReadResult{value, Available}, source, sensitive);
}

QVariantMap Collector::numericField(const QString &key, const QString &path,
                                    const QString &unit, double divisor, int precision,
                                    bool allowNegative)
{
    const ReadResult result = readText(path);
    if (result.status != Available) {
        QVariantMap item = field(key, result, path);
        item.insert(QStringLiteral("reason"),
                    result.status == PermissionDenied ? QStringLiteral("read access denied")
                    : result.status == ReadError ? QStringLiteral("attribute could not be read")
                                                 : QStringLiteral("attribute is absent or empty"));
        return item;
    }
    static const QRegularExpression unsignedPattern(QStringLiteral("^[0-9]+$"));
    static const QRegularExpression signedPattern(QStringLiteral("^-?[0-9]+$"));
    bool ok = false;
    const qint64 raw = result.value.toLongLong(&ok);
    if (!ok || !(allowNegative ? signedPattern : unsignedPattern).match(result.value).hasMatch()) {
        QVariantMap item = field(key, {{}, ReadError}, path);
        item.insert(QStringLiteral("reason"), QStringLiteral("malformed numeric value"));
        return item;
    }
    const double scaled = static_cast<double>(raw) / divisor;
    const bool integral = precision < 0 && divisor == 1.0;
    QVariantMap item = field(key,
                             {integral ? QString::number(raw)
                                       : QString::number(scaled, 'f', precision < 0 ? 2 : precision),
                              Available},
                             path);
    item.insert(QStringLiteral("rawValue"),
                integral ? QVariant::fromValue<qint64>(raw) : QVariant(scaled));
    if (!unit.isEmpty())
        item.insert(QStringLiteral("unit"), unit);
    return item;
}

QVariantMap Collector::bytesField(const QString &key, const QString &path)
{
    const ReadResult result = readText(path);
    if (result.status != Available) {
        QVariantMap item = field(key, result, path);
        item.insert(QStringLiteral("reason"),
                    result.status == PermissionDenied ? QStringLiteral("read access denied")
                    : result.status == ReadError ? QStringLiteral("attribute could not be read")
                                                 : QStringLiteral("attribute is absent or empty"));
        return item;
    }
    static const QRegularExpression integerPattern(QStringLiteral("^[0-9]+$"));
    bool ok = false;
    const quint64 raw = result.value.toULongLong(&ok);
    if (!ok || !integerPattern.match(result.value).hasMatch()) {
        QVariantMap item = field(key, {{}, ReadError}, path);
        item.insert(QStringLiteral("reason"), QStringLiteral("malformed byte count"));
        return item;
    }
    QVariantMap item = field(key, {prettyBytes(raw), Available}, path);
    item.insert(QStringLiteral("rawValue"), QVariant::fromValue<quint64>(raw));
    item.insert(QStringLiteral("unit"), QStringLiteral("bytes"));
    return item;
}

QVariantMap Collector::device(const QString &id, const QString &category, const QString &name,
                   const QString &summary, const QString &source, const QVariantList &fields)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("category"), category},
            {QStringLiteral("name"), name.isEmpty() ? QStringLiteral("Unknown") : name},
            {QStringLiteral("summary"), summary},
            {QStringLiteral("subtitle"), summary},
            {QStringLiteral("source"), source},
            {QStringLiteral("status"), name.isEmpty() ? Unavailable : Available},
            {QStringLiteral("fields"), fields}};
}

QVariantList Collector::directoryFields(const QString &directory, const QStringList &names,
                             const QSet<QString> &sensitive)
{
    QVariantList result;
    for (const QString &name : names)
        result.append(textField(name, directory + QLatin1Char('/') + name, sensitive.contains(name)));
    return result;
}

QString Collector::prettyBytes(quint64 bytes)
{
    static const QStringList units{QStringLiteral("B"), QStringLiteral("KiB"),
                                   QStringLiteral("MiB"), QStringLiteral("GiB"),
                                   QStringLiteral("TiB"), QStringLiteral("PiB")};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', unit == 0 ? 0 : 2), units.at(unit));
}

QString Collector::osPrettyName()
{
    const ReadResult osRelease = readText(pathFor(QStringLiteral("/etc/os-release")));
    const QStringList lines = osRelease.value.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("PRETTY_NAME="))) {
            QString value = line.mid(12).trimmed();
            if (value.size() >= 2 && ((value.startsWith('"') && value.endsWith('"')) ||
                                      (value.startsWith('\'') && value.endsWith('\''))))
                value = value.mid(1, value.size() - 2);
            return value;
        }
    }
    return source.root.isEmpty() ? QSysInfo::prettyProductName() : QString();
}

QString Collector::uptimeText()
{
    const QString raw = readText(pathFor(QStringLiteral("/proc/uptime"))).value.section(QLatin1Char(' '), 0, 0);
    bool ok = false;
    const double value = raw.toDouble(&ok);
    if (!ok || !std::isfinite(value) || value < 0 || value >= static_cast<double>(std::numeric_limits<quint64>::max()))
        return {};
    const auto seconds = static_cast<quint64>(value);
    const quint64 days = seconds / 86400;
    const quint64 hours = (seconds % 86400) / 3600;
    const quint64 minutes = (seconds % 3600) / 60;
    return days ? QStringLiteral("%1 d %2 h %3 min").arg(days).arg(hours).arg(minutes)
                : QStringLiteral("%1 h %2 min").arg(hours).arg(minutes);
}

} // namespace neko::inventory
