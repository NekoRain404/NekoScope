pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
    readonly property var groups: [ui.t("全部设备", "All devices"), ...Array.from(new Set(sensors.readings.map(item => item.group)))]
    readonly property var visibleReadings: sensors.readings.filter(item =>
        (!favoritesOnly.checked || item.favorite) && (groupBox.currentIndex <= 0 || item.group === groups[groupBox.currentIndex]) &&
        (item.name + " " + item.group + " " + item.unit).toLowerCase().includes(search.text.toLowerCase()))
    PageHeader {
        ui: page.ui; title: page.ui.t("实时传感器", "Follow every signal")
        description: page.ui.t("关注温度与负载，保留当前会话的变化轨迹。", "Track temperature and activity throughout this session.")
        eyebrow: sensors.paused ? page.ui.t("监控 · 显示已暂停", "MONITORING · DISPLAY PAUSED") : page.ui.t("监控 · 实时采样", "MONITORING · LIVE SAMPLING")
        ActionButton { ui: page.ui; text: sensors.paused ? page.ui.t("继续显示", "Resume") : page.ui.t("暂停显示", "Pause"); onClicked: sensors.paused = !sensors.paused; objectName: "pauseSensorsButton" }
        ActionButton { ui: page.ui; text: page.ui.t("重置统计", "Reset statistics"); onClicked: sensors.resetStatistics(); objectName: "resetSensorsButton" }
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 15
        MetricCard { ui: page.ui; title: page.ui.t("CPU 使用率", "CPU usage"); value: page.ui.metric(sensors.summary.cpuUsage, 1); unit: "%"; detail: page.ui.t("从 /proc/stat 计算", "Calculated from /proc/stat") }
        MetricCard { ui: page.ui; title: page.ui.t("内存使用率", "Memory usage"); value: page.ui.metric(sensors.summary.memoryUsage, 1); unit: "%"; detail: page.ui.metric(sensors.summary.memoryUsedGiB, 1) + " / " + page.ui.metric(sensors.summary.memoryTotalGiB, 1) + " GiB"; tone: page.ui.green }
        MetricCard { ui: page.ui; title: page.ui.t("CPU 温度", "CPU temperature"); value: page.ui.metric(sensors.summary.cpuTemperature, 1); unit: "°C"; detail: page.ui.t("无可用通道时显示 —", "— indicates no readable channel"); tone: page.ui.warning }
    }
    Panel {
        ui: page.ui; Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            Label { text: page.ui.t("历史曲线", "Session history"); color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            ComboBox { id: seriesBox; model: [page.ui.t("CPU 使用率", "CPU usage"), page.ui.t("CPU 温度", "CPU temperature")]; Layout.preferredWidth: 165; objectName: "chartSeriesSelector" }
        }
        HistoryChart { ui: page.ui; samples: seriesBox.currentIndex === 0 ? sensors.cpuHistory : sensors.temperatureHistory; lineColor: seriesBox.currentIndex === 0 ? page.ui.accent : page.ui.warning; maximum: 100; unit: seriesBox.currentIndex === 0 ? "%" : "°C"; Layout.fillWidth: true; Layout.preferredHeight: 200 }
        Label { text: page.ui.t("最近采样：", "Last sample: ") + (sensors.summary.updatedAt || "—"); color: page.ui.muted; font.pixelSize: 9; Layout.fillWidth: true }
    }
    Panel {
        ui: page.ui; Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            Label { text: page.ui.t("传感器", "Sensors") + " · " + page.visibleReadings.length; color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            CheckBox { id: favoritesOnly; text: page.ui.t("仅关注", "Favorites"); font.pixelSize: 11; objectName: "favoritesOnlyFilter" }
            ComboBox { id: groupBox; model: page.groups; Layout.preferredWidth: 145; objectName: "sensorGroupFilter" }
            TextField { id: search; placeholderText: page.ui.t("搜索传感器…", "Search sensors…"); Layout.preferredWidth: 160; selectByMouse: true; objectName: "sensorSearch" }
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.preferredWidth: 30 }
            Label { text: page.ui.t("名称 / 来源", "Name / source"); color: page.ui.muted; font.pixelSize: 10; Layout.fillWidth: true }
            Repeater { model: [page.ui.t("当前", "Current"), page.ui.t("最低", "Min"), page.ui.t("最高", "Max"), page.ui.t("平均", "Average")]; delegate: Label { required property string modelData; text: modelData; color: page.ui.muted; font.pixelSize: 10; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 73 } }
        }
        ListView {
            id: sensorList
            objectName: "sensorList"
            Layout.fillWidth: true; Layout.preferredHeight: Math.min(480, Math.max(170, page.visibleReadings.length * 66)); clip: true
            model: page.visibleReadings
            ScrollBar.vertical: ScrollBar {}
            delegate: Item {
                id: sensorRow
                required property var modelData
                width: sensorList.width; height: 66
                RowLayout {
                    anchors.fill: parent; spacing: 8
                    Button {
                        id: favoriteButton
                        implicitWidth: 30; implicitHeight: 32
                        checkable: true; checked: sensorRow.modelData.favorite
                        Accessible.name: (checked ? page.ui.t("取消关注 ", "Unfavorite ") : page.ui.t("关注 ", "Favorite ")) + sensorRow.modelData.name
                        onClicked: sensors.toggleFavorite(sensorRow.modelData.id)
                        contentItem: Icon { name: "star"; color: favoriteButton.checked ? page.ui.accent : page.ui.muted; opacity: favoriteButton.checked ? 1 : 0.45 }
                        background: Rectangle { color: "transparent"; radius: 5; border.color: favoriteButton.activeFocus ? page.ui.accent : "transparent" }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 5
                        Label { text: sensorRow.modelData.name; color: page.ui.text; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: sensorRow.modelData.group + " · " + sensorRow.modelData.source; color: page.ui.muted; font.pixelSize: 9; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    }
                    Repeater {
                        model: [sensorRow.modelData.value, sensorRow.modelData.min, sensorRow.modelData.max, sensorRow.modelData.avg]
                        delegate: Label {
                            required property var modelData
                            required property int index
                            text: sensorRow.modelData.status === "available" ? page.ui.metric(modelData, 1, true) + " " + sensorRow.modelData.unit : "—"
                            color: index === 0 ? page.ui.text : page.ui.secondary; font.pixelSize: 11; font.weight: index === 0 ? Font.DemiBold : Font.Normal
                            horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 73; elide: Text.ElideRight
                        }
                    }
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: page.ui.border }
                HoverHandler { id: rowHover }
                ToolTip.visible: rowHover.hovered
                ToolTip.delay: 700
                ToolTip.text: page.ui.status(modelData.status) + "\n" + modelData.source + "\n" + (modelData.updatedAt || "")
            }
            Label { anchors.centerIn: parent; visible: !page.visibleReadings.length; text: page.ui.t("没有匹配的传感器。试试调整筛选条件。", "No matching sensors. Try changing the filters."); color: page.ui.muted; font.pixelSize: 12 }
        }
    }
}
