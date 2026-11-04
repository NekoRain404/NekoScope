#include "ReportBuilder.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QRegularExpression>

namespace neko {
namespace {
bool privateKey(const QString &key)
{
    static const QRegularExpression pattern("serial|uuid|machine.?id|hostname|host.?name|mac|address|wwn|home|user.?name", QRegularExpression::CaseInsensitiveOption);
    return pattern.match(key).hasMatch();
}
QVariant redact(const QVariant &value, bool sensitive)
{
    if (sensitive) return QStringLiteral("[redacted]");
    if (value.metaType().id() == QMetaType::QVariantMap) {
        auto map = value.toMap();
        const bool secretValue = map.value("sensitive").toBool() || privateKey(map.value("key").toString());
        for (auto it = map.begin(); it != map.end(); ++it)
            it.value() = redact(it.value(), privateKey(it.key()) || (it.key() == "value" && secretValue)
                                || it.key() == "output");
        return map;
    }
    if (value.metaType().id() == QMetaType::QVariantList) {
        QVariantList list;
        for (const auto &entry : value.toList()) list.append(redact(entry, false));
        return list;
    }
    if (value.metaType().id() == QMetaType::QString) {
        QString text = value.toString();
        text.replace(QRegularExpression("/home/[^/\\s]+"), "/home/[redacted]");
        return text;
    }
    return value;
}
QString textValue(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QVariantMap || value.metaType().id() == QMetaType::QVariantList)
        return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
    return value.toString();
}
QString csvCell(QString value)
{
    // Spreadsheet programs interpret these prefixes as formulae, even inside quotes.
    if (!value.isEmpty() && QStringLiteral("=+-@\t\r").contains(value.front())) value.prepend('\'');
    value.replace('"', "\"\"");
    return '"' + value + '"';
}
// Markdown table cells cannot contain raw pipes, line breaks or unescaped inline HTML.
QString markdownCell(QString value)
{
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("&"), QStringLiteral("&amp;"));
    value.replace(QStringLiteral("<"), QStringLiteral("&lt;"));
    value.replace(QStringLiteral(">"), QStringLiteral("&gt;"));
    value.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    value.replace(QChar('\r'), QChar('\n'));
    value.replace(QChar('\n'), QStringLiteral("<br>"));
    value.replace(QStringLiteral("|"), QStringLiteral("\\|"));
    return value;
}

using ReportRow = QStringList;

// All three table formats share the same section/device/field/value/status/source rows.
QList<ReportRow> reportRows(const QVariantMap &document)
{
    QList<ReportRow> rows;
    const auto system = document.value(QStringLiteral("system")).toMap();
    for (auto it = system.cbegin(); it != system.cend(); ++it) {
        rows.append({QStringLiteral("System"), QString(), it.key(), textValue(it.value()),
                     QString(), QString()});
    }
    for (const auto &entry : document.value(QStringLiteral("devices")).toList()) {
        const auto device = entry.toMap();
        for (const auto &fieldValue : device.value(QStringLiteral("fields")).toList()) {
            const auto field = fieldValue.toMap();
            rows.append({device.value(QStringLiteral("category")).toString(),
                         device.value(QStringLiteral("name")).toString(),
                         field.value(QStringLiteral("key")).toString(),
                         textValue(field.value(QStringLiteral("value"))),
                         field.value(QStringLiteral("status")).toString(),
                         field.value(QStringLiteral("source")).toString()});
        }
    }
    for (const auto &entry : document.value(QStringLiteral("sensors")).toList()) {
        const auto sensor = entry.toMap();
        rows.append({QStringLiteral("Sensor"), sensor.value(QStringLiteral("group")).toString(),
                     sensor.value(QStringLiteral("name")).toString(),
                     textValue(sensor.value(QStringLiteral("value"))) + QLatin1Char(' ') +
                         sensor.value(QStringLiteral("unit")).toString(),
                     sensor.value(QStringLiteral("status")).toString(),
                     sensor.value(QStringLiteral("source")).toString()});
    }
    for (const auto &entry : document.value(QStringLiteral("tests")).toList()) {
        const auto test = entry.toMap();
        rows.append({QStringLiteral("Test"), test.value(QStringLiteral("kind")).toString(),
                     test.value(QStringLiteral("type")).toString(), textValue(test),
                     test.value(QStringLiteral("status")).toString(),
                     test.value(QStringLiteral("engine")).toString()});
    }
    return rows;
}

QString htmlReport(const QVariantMap &document)
{
    QString out = QStringLiteral("<!doctype html><html lang=\"en\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><title>NekoScope report</title><style>body{font:14px system-ui;max-width:1100px;margin:40px auto;padding:20px;color:#272435}table{border-collapse:collapse;width:100%;margin:20px 0}td,th{padding:10px;text-align:left;border-bottom:1px solid #ddd;overflow-wrap:anywhere}pre{white-space:pre-wrap}</style><body><h1>NekoScope</h1><p>Real hardware snapshot · %1 · %2</p><table><thead><tr><th>Section</th><th>Device</th><th>Field</th><th>Value</th><th>Status</th><th>Source</th></tr></thead><tbody>")
        .arg(document.value(QStringLiteral("generatedAt")).toString().toHtmlEscaped(),
             document.value(QStringLiteral("redacted")).toBool()
                 ? QStringLiteral("Redacted") : QStringLiteral("Includes identifiers"));
    for (const auto &row : reportRows(document)) {
        out += QStringLiteral("<tr>");
        for (const auto &cell : row) out += QStringLiteral("<td>") + cell.toHtmlEscaped() + QStringLiteral("</td>");
        out += QStringLiteral("</tr>\n");
    }
    out += QStringLiteral("</tbody></table></body></html>");
    return out;
}

QString csvReport(const QVariantMap &document)
{
    QString out = QChar(0xfeff) + QStringLiteral("section,device,field,value,status,source\r\n");
    for (const auto &row : reportRows(document)) {
        QStringList escaped;
        escaped.reserve(row.size());
        for (const auto &cell : row) escaped.append(csvCell(cell));
        out += escaped.join(QLatin1Char(',')) + QStringLiteral("\r\n");
    }
    return out;
}

QString markdownReport(const QVariantMap &document)
{
    QString out = QStringLiteral("# NekoScope hardware report\n\n");
    out += QStringLiteral("- Collected: %1\n")
               .arg(document.value(QStringLiteral("generatedAt")).toString());
    out += QStringLiteral("- Data mode: %1\n")
               .arg(document.value(QStringLiteral("dataMode")).toString());
    out += document.value(QStringLiteral("redacted")).toBool()
               ? QStringLiteral("- Privacy: sensitive identifiers are redacted\n")
               : QStringLiteral("- Privacy: includes identifiers, not redacted\n");
    out += QStringLiteral("\n| Section | Device | Field | Value | Status | Source |\n");
    out += QStringLiteral("| --- | --- | --- | --- | --- | --- |\n");
    for (const auto &row : reportRows(document)) {
        QStringList escaped;
        escaped.reserve(row.size());
        for (const auto &cell : row) escaped.append(markdownCell(cell));
        out += QStringLiteral("| ") + escaped.join(QStringLiteral(" | ")) + QStringLiteral(" |\n");
    }
    return out;
}
}
QVariantMap ReportBuilder::document(const QVariantMap &system, const QVariantList &devices,
                                   const QVariantList &sensors, const QVariantList &history,
                                   bool includeSensitive, const QStringList &categories)
{
    QVariantList selected;
    for (const auto &entry : devices) {
        if (categories.isEmpty() || categories.contains(entry.toMap().value("category").toString()))
            selected.append(entry);
    }
    QVariantMap result{{"schemaVersion", 1}, {"application", "NekoScope"}, {"dataMode", "live"},
        {"generatedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"redacted", !includeSensitive}, {"system", system}, {"devices", selected},
        {"sensors", sensors}, {"tests", history}};
    return includeSensitive ? result : redact(result, false).toMap();
}
QString ReportBuilder::serialize(const QVariantMap &document, const QString &format)
{
    if (format == QStringLiteral("json"))
        return QString::fromUtf8(QJsonDocument::fromVariant(document).toJson(QJsonDocument::Indented));
    if (format == QStringLiteral("html")) return htmlReport(document);
    if (format == QStringLiteral("csv")) return csvReport(document);
    if (format == QStringLiteral("markdown") || format == QStringLiteral("md"))
        return markdownReport(document);
    return {};
}
}
