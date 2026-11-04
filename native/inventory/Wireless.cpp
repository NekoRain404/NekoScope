#include "InventoryContext.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <cmath>

namespace neko::inventory {

namespace {

bool parseMeasurement(QString token, double &value)
{
    if (token.endsWith(QLatin1Char('.')))
        token.chop(1);
    bool ok = false;
    value = token.toDouble(&ok);
    return ok && std::isfinite(value);
}

QString measurementText(double value)
{
    return value == std::floor(value) ? QString::number(static_cast<qint64>(value))
                                      : QString::number(value, 'f', 1);
}

} // namespace

void Collector::appendWireless(InventorySnapshot &snapshot)
{
    const QString netRoot = pathFor(QStringLiteral("/sys/class/net"));
    const QString procPath = pathFor(QStringLiteral("/proc/net/wireless"));
    const ReadResult stats = readText(procPath);
    for (const QString &entry : QDir(netRoot).entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                        QDir::Name)) {
        const QString path = netRoot + QLatin1Char('/') + entry;
        // Both the legacy wireless-extension directory and phy80211 identify a WLAN device.
        if (!QFileInfo(path + QStringLiteral("/wireless")).exists() &&
            !QFileInfo(path + QStringLiteral("/phy80211")).exists())
            continue;

        QVariantList fields{
            valueField(QStringLiteral("Interface"), entry, path),
            textField(QStringLiteral("MAC address"), path + QStringLiteral("/address"), true),
        };
        QString phy = QFileInfo(path + QStringLiteral("/phy80211")).symLinkTarget();
        if (!phy.isEmpty())
            phy = QFileInfo(phy).fileName();
        if (phy.isEmpty()) {
            const QString canonical = QFileInfo(path + QStringLiteral("/phy80211"))
                                          .canonicalFilePath();
            if (!canonical.isEmpty())
                phy = QFileInfo(canonical).fileName();
        }
        if (phy.isEmpty()) {
            QVariantMap item = field(QStringLiteral("PHY"),
                                     {{}, Unavailable}, path + QStringLiteral("/phy80211"));
            item.insert(QStringLiteral("reason"),
                        QStringLiteral("phy80211 link is not exposed"));
            fields.append(item);
        } else {
            fields.append(valueField(QStringLiteral("PHY"), phy,
                                     path + QStringLiteral("/phy80211")));
        }
        const QString driverTarget = QFileInfo(path + QStringLiteral("/device/driver"))
                                         .symLinkTarget();
        const QString driver = driverTarget.isEmpty() ? QString()
                                                      : QFileInfo(driverTarget).fileName();
        fields.append(valueField(QStringLiteral("Driver"), driver,
                                 path + QStringLiteral("/device/driver")));

        QStringList tokens;
        bool rowFound = false;
        if (stats.status == Available) {
            for (const QString &line : stats.value.split(QLatin1Char('\n'))) {
                const qsizetype colon = line.indexOf(QLatin1Char(':'));
                if (colon <= 0)
                    continue;
                if (line.left(colon).trimmed() != entry)
                    continue;
                tokens = line.mid(colon + 1).split(QRegularExpression(QStringLiteral("\\s+")),
                                                   Qt::SkipEmptyParts);
                rowFound = true;
                break;
            }
        }

        const auto appendUnavailable = [&](const QString &key, const QString &reason) {
            QVariantMap item = field(key, {{}, stats.status == Available ? Unavailable : stats.status},
                                     procPath);
            item.insert(QStringLiteral("reason"), reason);
            fields.append(item);
        };
        if (stats.status != Available) {
            for (const QString &key : {QStringLiteral("Link quality"), QStringLiteral("Signal level"),
                                       QStringLiteral("Noise level")})
                appendUnavailable(key, QStringLiteral("wireless statistics are not exposed by "
                                                      "/proc/net/wireless"));
        } else if (!rowFound) {
            for (const QString &key : {QStringLiteral("Link quality"), QStringLiteral("Signal level"),
                                       QStringLiteral("Noise level")})
                appendUnavailable(key, QStringLiteral("interface is not present in /proc/net/wireless "
                                                      "(link may be down)"));
        } else {
            const auto appendMeasurement = [&](const QString &key, qsizetype index,
                                               const QString &unit, bool rejectSentinel) {
                if (tokens.size() <= index) {
                    appendUnavailable(key, QStringLiteral("wireless statistics row is incomplete"));
                    return;
                }
                double value = 0.0;
                if (!parseMeasurement(tokens.at(index), value)) {
                    QVariantMap item = field(key, {{}, ReadError}, procPath);
                    item.insert(QStringLiteral("reason"),
                                QStringLiteral("malformed wireless statistics value"));
                    fields.append(item);
                    return;
                }
                if (rejectSentinel && value <= -256.0) {
                    appendUnavailable(key, QStringLiteral("kernel reports no measurement "
                                                          "(sentinel value)"));
                    return;
                }
                QVariantMap item = valueField(key, measurementText(value), procPath);
                if (!unit.isEmpty())
                    item.insert(QStringLiteral("unit"), unit);
                item.insert(QStringLiteral("rawValue"), value);
                fields.append(item);
            };
            appendMeasurement(QStringLiteral("Link quality"), 1, QString(), false);
            appendMeasurement(QStringLiteral("Signal level"), 2, QStringLiteral("dBm"), true);
            appendMeasurement(QStringLiteral("Noise level"), 3, QStringLiteral("dBm"), true);
        }

        snapshot.devices.append(device(QStringLiteral("wifi:") + entry, QStringLiteral("网络"),
                                       entry,
                                       phy.isEmpty() ? QStringLiteral("wireless interface")
                                                     : QStringLiteral("%1 wireless interface").arg(phy),
                                       path, fields));
    }
}

} // namespace neko::inventory
