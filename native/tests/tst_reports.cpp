#include "app/ReportBuilder.h"
#include <QJsonDocument>
#include <QtTest>

class ReportsTest : public QObject {
    Q_OBJECT
private slots:
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
};
QTEST_GUILESS_MAIN(ReportsTest)
#include "tst_reports.moc"
