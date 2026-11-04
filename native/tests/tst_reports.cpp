#include "app/ReportBuilder.h"
#include "app/ReportService.h"
#include "app/HardwareService.h"
#include "core/SensorService.h"
#include "core/TestRunner.h"
#include <QJsonDocument>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class ReportsTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void protectsIdentityAndFiltersCategories()
    {
        QVariantMap system{{"hostname", "private-host"}, {"kernel", "6.12"}};
        QVariantList devices{
            QVariantMap{{"id", "cpu"}, {"category", "CPU"}, {"name", "CPU"},
                {"fields", QVariantList{QVariantMap{{"key", "serial"}, {"value", "SECRET"}, {"sensitive", true}}}}},
            QVariantMap{{"id", "net"}, {"category", "Network"}, {"name", "Adapter"}}};
        const auto doc = neko::ReportBuilder::document(system, devices, {}, {}, false, {"CPU"});
        const auto json = neko::ReportBuilder::serialize(doc, "json");
        QVERIFY(!json.contains("private-host"));
        QVERIFY(!json.contains("SECRET"));
        QCOMPARE(doc.value("devices").toList().size(), 1);
        QVERIFY(!QJsonDocument::fromJson(json.toUtf8()).isNull());
        const auto full = neko::ReportBuilder::serialize(
            neko::ReportBuilder::document(system, devices, {}, {}, true, {}), "json");
        QVERIFY(full.contains("private-host"));
        QVERIFY(full.contains("SECRET"));
    }
    void escapesHtmlAndCsv()
    {
        QVariantList fields{QVariantMap{{"key", "Name"}, {"value", "<script>\"a,b\"\nnext"}}};
        QVariantList devices{QVariantMap{{"name", "disk"}, {"category", "Storage"}, {"fields", fields}}};
        const auto doc = neko::ReportBuilder::document({}, devices, {}, {}, true, {});
        const auto html = neko::ReportBuilder::serialize(doc, "html");
        QVERIFY(!html.contains("<script>"));
        QVERIFY(html.contains("&lt;script&gt;"));
        const auto csv = neko::ReportBuilder::serialize(doc, "csv");
        QVERIFY(csv.contains("\"\"a,b\"\""));
        QVERIFY(csv.contains("\r\n"));
        QVERIFY(neko::ReportBuilder::serialize(doc, "exe").isEmpty());
    }
    void markdownEscapesTableCells()
    {
        QVariantList fields{QVariantMap{{"key", "Path"}, {"value", "a|b\\c\nd<e>f&g"}}};
        QVariantList devices{QVariantMap{{"name", "disk"}, {"category", "Storage"}, {"fields", fields}}};
        const auto doc = neko::ReportBuilder::document({}, devices, {}, {}, true, {});
        const auto markdown = neko::ReportBuilder::serialize(doc, "markdown");
        QVERIFY(!markdown.isEmpty());
        QVERIFY(markdown.contains(QStringLiteral("a\\|b\\\\c<br>d&lt;e&gt;f&amp;g")));
        QVERIFY(!markdown.contains(QStringLiteral("a|b")));
        QVERIFY(!markdown.contains(QStringLiteral("<e>")));
        QCOMPARE(neko::ReportBuilder::serialize(doc, "md"), markdown);
    }
    void markdownHasReadableStructure()
    {
        QVariantList devices{QVariantMap{
            {"name", "disk"}, {"category", "Storage"},
            {"fields", QVariantList{QVariantMap{{"key", "Model"}, {"value", "FastDisk"},
                                                {"status", "ok"}, {"source", "smartctl"}}}}}};
        QVariantList sensors{QVariantMap{{"group", "CPU"}, {"name", "temp"}, {"value", 42},
                                         {"unit", "C"}, {"status", "ok"}, {"source", "hwmon"}}};
        QVariantList history{QVariantMap{{"kind", "benchmark"}, {"type", "single"},
                                         {"status", "completed"}, {"engine", "sysbench"}}};
        const auto doc = neko::ReportBuilder::document({}, devices, sensors, history, true, {});
        const auto markdown = neko::ReportBuilder::serialize(doc, "markdown");
        QVERIFY(markdown.startsWith(QStringLiteral("# NekoScope")));
        QVERIFY(markdown.contains(QStringLiteral("- Collected: ")));
        QVERIFY(markdown.contains(QStringLiteral("- Privacy: includes identifiers")));
        QVERIFY(markdown.contains(QStringLiteral("| Section | Device | Field | Value | Status | Source |")));
        QVERIFY(markdown.contains(QStringLiteral("| Storage | disk | Model | FastDisk | ok | smartctl |")));
        QVERIFY(markdown.contains(QStringLiteral("| Sensor | CPU | temp | 42 C | ok | hwmon |")));
        QVERIFY(markdown.contains(QStringLiteral("| Test | benchmark | single |")));
    }
    void markdownAppliesRedactionAndCategoryFilter()
    {
        QVariantMap system{{"hostname", "private-host"}, {"kernel", "6.12"}};
        QVariantList devices{
            QVariantMap{{"id", "cpu"}, {"category", "CPU"}, {"name", "CPU"},
                {"fields", QVariantList{QVariantMap{{"key", "serial"}, {"value", "SECRET"}, {"sensitive", true}}}}},
            QVariantMap{{"id", "net"}, {"category", "Network"}, {"name", "Adapter"}}};
        const auto doc = neko::ReportBuilder::document(system, devices, {}, {}, false, {"CPU"});
        const auto markdown = neko::ReportBuilder::serialize(doc, "markdown");
        QVERIFY(!markdown.contains("private-host"));
        QVERIFY(!markdown.contains("SECRET"));
        QVERIFY(!markdown.contains("Adapter"));
        QVERIFY(markdown.contains("[redacted]"));
        QVERIFY(markdown.contains(QStringLiteral("- Privacy: sensitive identifiers are redacted")));
        QVERIFY(markdown.contains("| 6.12 |"));
    }
    void otherFormatsKeepTheirContract()
    {
        QVariantList fields{QVariantMap{{"key", "Model"}, {"value", "FastDisk"}}};
        QVariantList devices{QVariantMap{{"name", "disk"}, {"category", "Storage"}, {"fields", fields}}};
        const auto doc = neko::ReportBuilder::document({}, devices, {}, {}, true, {});
        QVERIFY(!QJsonDocument::fromJson(
                     neko::ReportBuilder::serialize(doc, "json").toUtf8()).isNull());
        const auto html = neko::ReportBuilder::serialize(doc, "html");
        QVERIFY(html.startsWith(QStringLiteral("<!doctype html>")));
        QVERIFY(html.endsWith(QStringLiteral("</body></html>")));
        const auto csv = neko::ReportBuilder::serialize(doc, "csv");
        QVERIFY(csv.startsWith(QChar(0xfeff)));
        QVERIFY(csv.contains(QStringLiteral("section,device,field,value,status,source\r\n")));
        QVERIFY(csv.contains(QStringLiteral("\"Storage\",\"disk\",\"Model\",\"FastDisk\"")));
    }
    void formatsListsSupportedOutputs()
    {
        neko::HardwareService hardware;
        neko::SensorService sensors;
        neko::TestRunner runner(nullptr, {}, {}, QStringLiteral("/proc"),
                                settingsDir_.filePath(QStringLiteral("benchmark-history.json")));
        neko::ReportService service(hardware, sensors, runner);
        QCOMPARE(service.formats(),
                 QStringList({QStringLiteral("html"), QStringLiteral("json"),
                              QStringLiteral("csv"), QStringLiteral("markdown")}));
    }
private:
    QTemporaryDir settingsDir_;
};

void ReportsTest::initTestCase()
{
    QVERIFY(settingsDir_.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("NekoScopeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("Reports"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir_.path());
}

QTEST_GUILESS_MAIN(ReportsTest)
#include "tst_reports.moc"
