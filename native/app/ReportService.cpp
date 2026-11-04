#include "ReportService.h"
#include "ReportBuilder.h"
#include "HardwareService.h"
#include "core/SensorService.h"
#include "core/TestRunner.h"
#include <QSaveFile>

namespace neko {
ReportService::ReportService(HardwareService &hardware, SensorService &sensors, TestRunner &tests, QObject *parent)
    : QObject(parent), hardware_(hardware), sensors_(sensors), tests_(tests) {}
QString ReportService::preview(const QString &format, bool includeSensitive, const QStringList &categories) const
{
    return ReportBuilder::serialize(ReportBuilder::document(hardware_.system(), hardware_.devices(),
        sensors_.readings(), tests_.history(), includeSensitive, categories), format);
}
bool ReportService::save(const QUrl &url, const QString &format, bool includeSensitive, const QStringList &categories)
{
    error_.clear();
    if (!url.isLocalFile() || url.toLocalFile().isEmpty()) error_ = QStringLiteral("Choose a local file");
    const QString content = preview(format, includeSensitive, categories);
    if (content.isEmpty()) error_ = QStringLiteral("Unsupported report format");
    if (!error_.isEmpty()) { emit changed(); return false; }
    QSaveFile file(url.toLocalFile());
    const QByteArray bytes = content.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error_ = file.errorString();
        emit changed();
        return false;
    }
    lastPath_ = url.toLocalFile();
    emit changed();
    return true;
}
}
