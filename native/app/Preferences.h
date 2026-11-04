#pragma once
#include <QObject>
#include <QSettings>

namespace neko {
class Preferences final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY changed)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY changed)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY changed)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY changed)
public:
    explicit Preferences(QObject *parent = nullptr);
    explicit Preferences(const QString &settingsPath, QObject *parent = nullptr);
    QString theme() const;
    QString language() const;
    QString accent() const;
    bool reducedMotion() const;
    bool systemDark() const { return systemDark_; }
    int schemaVersion() const { return schemaVersion_; }
    void setSystemDark(bool value);
    void setTheme(const QString &value);
    void setLanguage(const QString &value);
    void setAccent(const QString &value);
    void setReducedMotion(bool value);
signals:
    void changed();
private:
    void initialize();
    void save(const QString &key, const QVariant &value);
    QSettings settings_;
    int schemaVersion_ = 1;
    bool systemDark_ = false;
};
}
