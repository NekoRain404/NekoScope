#pragma once
#include <QObject>
#include <QUrl>
#include <QStringList>

namespace neko {
class HardwareService;
class SensorService;
class TestRunner;
class ReportService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString lastPath READ lastPath NOTIFY changed)
    Q_PROPERTY(QStringList formats READ formats CONSTANT)
public:
    ReportService(HardwareService &hardware, SensorService &sensors, TestRunner &tests, QObject *parent = nullptr);
    QString error() const { return error_; }
    QString lastPath() const { return lastPath_; }
    QStringList formats() const { return {QStringLiteral("html"), QStringLiteral("json"),
                                          QStringLiteral("csv"), QStringLiteral("markdown")}; }
    Q_INVOKABLE QString preview(const QString &format, bool includeSensitive, const QStringList &categories) const;
    Q_INVOKABLE bool save(const QUrl &url, const QString &format, bool includeSensitive, const QStringList &categories);
signals:
    void changed();
private:
    HardwareService &hardware_;
    SensorService &sensors_;
    TestRunner &tests_;
    QString error_, lastPath_;
};
}
