#include "../InventoryCollector.h"

#include <QTest>

class InventoryTest final : public QObject {
    Q_OBJECT

private slots:
    void collectsRealSystemMetadata()
    {
        const neko::InventorySnapshot snapshot = neko::collectInventory();
        QVERIFY(!snapshot.system.isEmpty());
        QVERIFY(!snapshot.system.value(QStringLiteral("os")).toString().isEmpty());
        QVERIFY(!snapshot.system.value(QStringLiteral("kernel")).toString().isEmpty());
        QVERIFY(!snapshot.system.value(QStringLiteral("architecture")).toString().isEmpty());
        QVERIFY(!snapshot.system.value(QStringLiteral("hostname")).toString().isEmpty());
        QVERIFY(!snapshot.system.value(QStringLiteral("logicalCpus")).toString().isEmpty());
        QVERIFY(snapshot.system.contains(QStringLiteral("memoryTotal")));
        QVERIFY(!snapshot.devices.isEmpty());
    }

    void inventoryFieldsHaveProvenanceAndStatus()
    {
        const neko::InventorySnapshot snapshot = neko::collectInventory();
        bool foundSource = false;
        for (const QVariant &item : snapshot.devices) {
            const QVariantMap device = item.toMap();
            QVERIFY(!device.value(QStringLiteral("id")).toString().isEmpty());
            QVERIFY(!device.value(QStringLiteral("category")).toString().isEmpty());
            QVERIFY(!device.value(QStringLiteral("source")).toString().isEmpty());
            QVERIFY(device.value(QStringLiteral("status")).toString() == QStringLiteral("available") ||
                    device.value(QStringLiteral("status")).toString() == QStringLiteral("permission-denied") ||
                    device.value(QStringLiteral("status")).toString() == QStringLiteral("unavailable"));
            const QVariantList fields = device.value(QStringLiteral("fields")).toList();
            QVERIFY(!fields.isEmpty());
            for (const QVariant &fieldValue : fields) {
                const QVariantMap field = fieldValue.toMap();
                QVERIFY(field.contains(QStringLiteral("key")));
                QVERIFY(field.contains(QStringLiteral("value")));
                QVERIFY(!field.value(QStringLiteral("source")).toString().isEmpty());
                QVERIFY(field.contains(QStringLiteral("sensitive")));
                const QString status = field.value(QStringLiteral("status")).toString();
                QVERIFY(status == QStringLiteral("available") || status == QStringLiteral("permission-denied") ||
                        status == QStringLiteral("unavailable"));
                foundSource = foundSource || field.value(QStringLiteral("source")).toString().startsWith('/');
            }
        }
        QVERIFY(foundSource);
    }

    void doesNotReturnPrototypeHardwareNames()
    {
        const neko::InventorySnapshot snapshot = neko::collectInventory();
        for (const QVariant &item : snapshot.devices) {
            const QString name = item.toMap().value(QStringLiteral("name")).toString();
            QVERIFY(!name.contains(QStringLiteral("Ryzen 9 7950X"), Qt::CaseInsensitive));
            QVERIFY(!name.contains(QStringLiteral("GeForce RTX 4080"), Qt::CaseInsensitive));
            QVERIFY(!name.contains(QStringLiteral("Samsung 990 PRO"), Qt::CaseInsensitive));
        }
    }
};

QTEST_GUILESS_MAIN(InventoryTest)
#include "tst_inventory.moc"
