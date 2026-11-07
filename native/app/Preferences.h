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
    Q_PROPERTY(int sensorIntervalMs READ sensorIntervalMs WRITE setSensorIntervalMs NOTIFY changed)
    Q_PROPERTY(int historySamples READ historySamples WRITE setHistorySamples NOTIFY changed)
    Q_PROPERTY(QString temperatureUnit READ temperatureUnit WRITE setTemperatureUnit NOTIFY changed)
    Q_PROPERTY(int startPage READ startPage WRITE setStartPage NOTIFY changed)
    Q_PROPERTY(bool showDiagnostics READ showDiagnostics WRITE setShowDiagnostics NOTIFY changed)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY changed)
public:
    explicit Preferences(QObject *parent = nullptr);
    explicit Preferences(const QString &settingsPath, QObject *parent = nullptr);
    QString theme() const;
    QString language() const;
    QString accent() const;
    bool reducedMotion() const;
    int sensorIntervalMs() const;
    int historySamples() const;
    QString temperatureUnit() const;
    int startPage() const;
    bool showDiagnostics() const;
    bool systemDark() const { return systemDark_; }
    int schemaVersion() const { return schemaVersion_; }
    void setSystemDark(bool value);
    void setTheme(const QString &value);
    void setLanguage(const QString &value);
    void setAccent(const QString &value);
    void setReducedMotion(bool value);
    void setSensorIntervalMs(int value);
    void setHistorySamples(int value);
    void setTemperatureUnit(const QString &value);
    void setStartPage(int value);
    void setShowDiagnostics(bool value);
    Q_INVOKABLE void reset();
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
