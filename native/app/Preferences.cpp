#include "Preferences.h"
#include <QLocale>
#include <QRegularExpression>

namespace neko {
Preferences::Preferences(QObject *parent) : QObject(parent) { initialize(); }
Preferences::Preferences(const QString &path, QObject *parent)
    : QObject(parent), settings_(path, QSettings::IniFormat) { initialize(); }

void Preferences::initialize()
{
    // Only stamp the schema when the store has no version yet; future
    // migrations must be driven by the value read here.
    if (!settings_.contains("schemaVersion")) {
        settings_.setValue("schemaVersion", schemaVersion_);
        return;
    }
    schemaVersion_ = settings_.value("schemaVersion").toInt();
}
QString Preferences::theme() const
{
    const auto value = settings_.value("appearance/theme", "system").toString();
    return QStringList{"light", "dark", "system"}.contains(value) ? value : QStringLiteral("system");
}
QString Preferences::language() const
{
    const auto fallback = QLocale::system().language() == QLocale::Chinese ? "zh_CN" : "en";
    const auto value = settings_.value("appearance/language", fallback).toString();
    return QStringList{"zh_CN", "en"}.contains(value) ? value : QString::fromLatin1(fallback);
}
QString Preferences::accent() const
{
    const auto value = settings_.value("appearance/accent", "#9784e8").toString();
    return QRegularExpression("^#[0-9a-fA-F]{6}$").match(value).hasMatch() ? value : QStringLiteral("#9784e8");
}
bool Preferences::reducedMotion() const { return settings_.value("appearance/reducedMotion", false).toBool(); }
void Preferences::save(const QString &key, const QVariant &value)
{
    if (settings_.value(key) == value) return;
    settings_.setValue(key, value);
    settings_.sync();
    emit changed();
}
void Preferences::setTheme(const QString &value)
{
    if (QStringList{"light", "dark", "system"}.contains(value)) save("appearance/theme", value);
}
void Preferences::setLanguage(const QString &value)
{
    if (QStringList{"zh_CN", "en"}.contains(value)) save("appearance/language", value);
}
void Preferences::setAccent(const QString &value)
{
    if (QRegularExpression("^#[0-9a-fA-F]{6}$").match(value).hasMatch()) save("appearance/accent", value);
}
void Preferences::setReducedMotion(bool value) { save("appearance/reducedMotion", value); }
void Preferences::setSystemDark(bool value)
{
    if (systemDark_ == value) return;
    systemDark_ = value;
    emit changed();
}
}
