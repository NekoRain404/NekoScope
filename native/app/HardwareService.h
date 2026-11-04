#pragma once
#include "InventoryCollector.h"
#include <QFutureWatcher>
#include <QObject>

namespace neko {
class HardwareService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY inventoryChanged)
    Q_PROPERTY(QVariantMap system READ system NOTIFY inventoryChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY inventoryChanged)
    Q_PROPERTY(QString lastUpdated READ lastUpdated NOTIFY inventoryChanged)
    Q_PROPERTY(QString renderBackend READ renderBackend NOTIFY renderBackendChanged)
public:
    explicit HardwareService(QObject *parent = nullptr);
    ~HardwareService() override;
    QVariantList devices() const { return snapshot_.devices; }
    QVariantMap system() const { return snapshot_.system; }
    bool busy() const { return watcher_.isRunning(); }
    QString error() const { return error_; }
    QString lastUpdated() const { return lastUpdated_; }
    QString renderBackend() const { return renderBackend_; }
    void setRenderBackend(const QString &value);
    Q_INVOKABLE void refresh();
signals:
    void inventoryChanged();
    void busyChanged();
    void renderBackendChanged();
private:
    InventorySnapshot snapshot_;
    QFutureWatcher<InventorySnapshot> watcher_;
    QString error_;
    QString lastUpdated_;
    QString renderBackend_ = QStringLiteral("initializing");
};
}
