#pragma once

#include <QVariantList>
#include <QVariantMap>

namespace neko {

struct InventorySnapshot {
    QVariantList devices;
    QVariantMap system;
};

InventorySnapshot collectInventory();

} // namespace neko
