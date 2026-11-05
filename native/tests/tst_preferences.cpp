#include "app/Preferences.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class PreferencesTest : public QObject {
    Q_OBJECT
private slots:
    void validatesAndPersists()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath("settings.ini");
        {
            neko::Preferences prefs(path);
            QCOMPARE(prefs.theme(), QString("system"));
            prefs.setTheme("dark");
            prefs.setLanguage("en");
            prefs.setAccent("#40a0b0");
            prefs.setReducedMotion(true);
            prefs.setSensorIntervalMs(2000);
            prefs.setHistorySamples(150);
            prefs.setTemperatureUnit("fahrenheit");
            prefs.setStartPage(3);
            prefs.setShowDiagnostics(false);
            prefs.setSensorIntervalMs(1234);
            prefs.setHistorySamples(9999);
            prefs.setTemperatureUnit("kelvin");
            prefs.setStartPage(99);
            prefs.setTheme("invalid");
            prefs.setLanguage("invalid");
            prefs.setAccent("red; url()");
            QCOMPARE(prefs.theme(), QString("dark"));
        }
        neko::Preferences restored(path);
        QCOMPARE(restored.theme(), QString("dark"));
        QCOMPARE(restored.language(), QString("en"));
        QCOMPARE(restored.accent(), QString("#40a0b0"));
        QVERIFY(restored.reducedMotion());
        QCOMPARE(restored.sensorIntervalMs(), 2000);
        QCOMPARE(restored.historySamples(), 150);
        QCOMPARE(restored.temperatureUnit(), QString("fahrenheit"));
        QCOMPARE(restored.startPage(), 3);
        QVERIFY(!restored.showDiagnostics());
        QSettings settings(path, QSettings::IniFormat);
        QCOMPARE(settings.value("schemaVersion").toInt(), 1);
    }
    void resetsToDefaults()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath("reset.ini");
        neko::Preferences prefs(path);
        prefs.setTheme("dark");
        prefs.setLanguage("en");
        prefs.setSensorIntervalMs(5000);
        prefs.setHistorySamples(300);
        prefs.setTemperatureUnit("fahrenheit");
        prefs.setStartPage(5);
        prefs.setShowDiagnostics(false);
        prefs.reset();
        QCOMPARE(prefs.theme(), QString("system"));
        QCOMPARE(prefs.sensorIntervalMs(), 1000);
        QCOMPARE(prefs.historySamples(), 90);
        QCOMPARE(prefs.temperatureUnit(), QString("celsius"));
        QCOMPARE(prefs.startPage(), 0);
        QVERIFY(prefs.showDiagnostics());
        QCOMPARE(prefs.schemaVersion(), 1);
    }
    void preservesExistingSchemaVersion()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath("legacy.ini");
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue("schemaVersion", 3);
            settings.sync();
        }
        {
            neko::Preferences prefs(path);
            QCOMPARE(prefs.schemaVersion(), 3);
            prefs.setTheme("dark");
        }
        QSettings settings(path, QSettings::IniFormat);
        QCOMPARE(settings.value("schemaVersion").toInt(), 3);
    }
};
QTEST_GUILESS_MAIN(PreferencesTest)
#include "tst_preferences.moc"
