#include "HardwareService.h"
#include <QDateTime>
#include <QtConcurrent>

namespace neko {
HardwareService::HardwareService(QObject *parent) : QObject(parent)
{
    connect(&watcher_, &QFutureWatcher<InventorySnapshot>::finished, this, [this] {
        try {
            snapshot_ = watcher_.result();
            error_.clear();
            lastUpdated_ = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        } catch (const std::exception &error) {
            error_ = QString::fromUtf8(error.what());
        } catch (...) {
            error_ = QStringLiteral("Inventory collection failed");
        }
        emit inventoryChanged();
        emit busyChanged();
    });
}
HardwareService::~HardwareService() { watcher_.waitForFinished(); }
void HardwareService::refresh()
{
    if (watcher_.isRunning()) return;
    watcher_.setFuture(QtConcurrent::run([] { return collectInventory(); }));
    emit busyChanged();
}
void HardwareService::setRenderBackend(const QString &value)
{
    if (renderBackend_ == value) return;
    renderBackend_ = value;
    emit renderBackendChanged();
}
}
