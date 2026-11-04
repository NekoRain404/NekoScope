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
        QSettings settings(path, QSettings::IniFormat);
        QCOMPARE(settings.value("schemaVersion").toInt(), 1);
    }
};
QTEST_GUILESS_MAIN(PreferencesTest)
#include "tst_preferences.moc"
