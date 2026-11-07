#pragma once

#include "../InventoryCollector.h"
#include <QDateTime>
#include <QString>

namespace neko::inventory {
// An alternate Linux namespace root supports offline diagnostic trees and fixtures.
// An empty root reads the running system. No host fallback is used for alternate roots.
struct InventorySource {
    QString root;
    QDateTime collectedAt = QDateTime::currentDateTimeUtc();
};

InventorySnapshot collectInventory(const InventorySource &source);
} // namespace neko::inventory
