#pragma once
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>

namespace neko {
class ReportBuilder final {
public:
    static QVariantMap document(const QVariantMap &system, const QVariantList &devices,
                                const QVariantList &sensors, const QVariantList &history,
                                bool includeSensitive, const QStringList &categories);
    static QString serialize(const QVariantMap &document, const QString &format);
};
}
