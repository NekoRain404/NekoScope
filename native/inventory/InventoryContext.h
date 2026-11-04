#pragma once

#include "InventorySource.h"
#include <QDir>
#include <QSet>
#include <utility>

namespace neko::inventory {
inline const QString Available = QStringLiteral("available");
inline const QString PermissionDenied = QStringLiteral("permission-denied");
inline const QString Unavailable = QStringLiteral("unavailable");
inline const QString ReadError = QStringLiteral("read-error");

struct ReadResult {
    QString value;
    QString status = Unavailable;
};

class Collector final {
public:
    explicit Collector(InventorySource input) : source(std::move(input)) {}
    InventorySnapshot run();

private:
    InventorySource source;
    QString pathFor(const QString &path) const
    { return source.root.isEmpty() ? path : QDir::cleanPath(source.root + path); }
    ReadResult readText(const QString &path);
    QString cleanId(QString value);
    QVariantMap field(const QString &key, const ReadResult &result, const QString &source, bool sensitive = false);
    QVariantMap textField(const QString &key, const QString &path, bool sensitive = false);
    QVariantMap valueField(const QString &key, const QString &value, const QString &source, bool sensitive = false);
    // Reads a single non-negative integer attribute, scales it by divisor and
    // reports rawValue in the same unit as the displayed value.
    QVariantMap numericField(const QString &key, const QString &path, const QString &unit,
                             double divisor = 1.0, int precision = -1,
                             bool allowNegative = false);
    // Reads an attribute holding a byte count and stores the pretty value plus
    // the exact byte count in rawValue.
    QVariantMap bytesField(const QString &key, const QString &path);
    QVariantMap device(const QString &id, const QString &category, const QString &name,
                       const QString &summary, const QString &source, const QVariantList &fields);
    QVariantList directoryFields(const QString &directory, const QStringList &names,
                                 const QSet<QString> &sensitive = {});
    QString prettyBytes(quint64 bytes);
    QString osPrettyName();
    QString uptimeText();
    void appendCpu(InventorySnapshot &snapshot);
    void appendMemory(InventorySnapshot &snapshot);
    void appendDmi(InventorySnapshot &snapshot);
    void appendStorage(InventorySnapshot &snapshot);
    void appendGpu(InventorySnapshot &snapshot);
    void appendNvme(InventorySnapshot &snapshot);
    void appendPci(InventorySnapshot &snapshot);
    void appendUsb(InventorySnapshot &snapshot);
    void appendNetwork(InventorySnapshot &snapshot);
    void appendWireless(InventorySnapshot &snapshot);
    void appendPower(InventorySnapshot &snapshot);
    void appendDisplays(InventorySnapshot &snapshot);
    void appendAudioCards(InventorySnapshot &snapshot);
};
} // namespace neko::inventory
