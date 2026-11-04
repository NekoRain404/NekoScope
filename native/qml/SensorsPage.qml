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
        ui: page.ui
        title: page.ui.t("实时传感器", "Follow every signal")
        description: page.ui.t("关注温度与负载，保留当前会话的变化轨迹。", "Track temperature and activity throughout this session.")
        eyebrow: sensors.paused ? page.ui.t("监控 · 显示已暂停", "MONITORING · DISPLAY PAUSED") : page.ui.t("监控 · 实时采样", "MONITORING · LIVE SAMPLING")
        ActionButton {
            ui: page.ui
            text: sensors.paused ? page.ui.t("继续显示", "Resume") : page.ui.t("暂停显示", "Pause")
            symbol: sensors.paused ? "play" : "pause"
            onClicked: sensors.paused = !sensors.paused
            objectName: "pauseSensorsButton"
        }
        ActionButton {
            ui: page.ui
            text: page.ui.t("重置统计", "Reset statistics")
            symbol: "refresh"
            onClicked: sensors.resetStatistics()
            objectName: "resetSensorsButton"
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: page.ui.space4
        MetricCard {
            ui: page.ui
            title: page.ui.t("CPU 使用率", "CPU usage")
            value: page.ui.metric(sensors.summary.cpuUsage, 1)
            unit: "%"
            detail: page.ui.t("从 /proc/stat 计算", "Calculated from /proc/stat")
        }
        MetricCard {
            ui: page.ui
            title: page.ui.t("内存使用率", "Memory usage")
            value: page.ui.metric(sensors.summary.memoryUsage, 1)
            unit: "%"
            detail: page.ui.metric(sensors.summary.memoryUsedGiB, 1) + " / " + page.ui.metric(sensors.summary.memoryTotalGiB, 1) + " GiB"
            tone: page.ui.green
        }
        MetricCard {
            ui: page.ui
            title: page.ui.t("CPU 温度", "CPU temperature")
            value: page.ui.metric(sensors.summary.cpuTemperature, 1)
            unit: "°C"
            detail: page.ui.t("无可用通道时显示 —", "— indicates no readable channel")
            tone: page.ui.warning
        }
    }
    Panel {
        ui: page.ui
        Layout.fillWidth: true
        SectionHeader {
            ui: page.ui
            title: page.ui.t("历史曲线", "Session history")
            detail: page.ui.t("最近采样：", "Last sample: ") + (sensors.summary.updatedAt || "—")
            SelectBox {
                id: seriesBox
                ui: page.ui
                model: [page.ui.t("CPU 使用率", "CPU usage"), page.ui.t("CPU 温度", "CPU temperature")]
                Layout.preferredWidth: 165
                objectName: "chartSeriesSelector"
            }
        }
        HistoryChart {
            ui: page.ui
            samples: seriesBox.currentIndex === 0 ? sensors.cpuHistory : sensors.temperatureHistory
            lineColor: seriesBox.currentIndex === 0 ? page.ui.accent : page.ui.warning
            maximum: 100
            unit: seriesBox.currentIndex === 0 ? "%" : "°C"
            Layout.fillWidth: true
            Layout.preferredHeight: 200
        }
    }
    Panel {
        ui: page.ui
        Layout.fillWidth: true
        spacing: page.ui.space3
        SectionHeader {
            ui: page.ui
            title: page.ui.t("传感器", "Sensors")
            detail: page.visibleReadings.length + page.ui.t(" 个通道", " channels")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space2
            CheckField {
                id: favoritesOnly
                ui: page.ui
                text: page.ui.t("仅关注", "Favorites")
                objectName: "favoritesOnlyFilter"
            }
            Item { Layout.fillWidth: true }
            SelectBox {
                id: groupBox
                ui: page.ui
                model: page.groups
                Layout.preferredWidth: 150
                objectName: "sensorGroupFilter"
            }
            SearchBox {
                id: search
                ui: page.ui
                placeholderText: page.ui.t("搜索传感器…", "Search sensors…")
                Layout.preferredWidth: 180
                    objectName: "sensorSearch"
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: page.ui.borderWidth
            color: page.ui.borderSubtle
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: page.ui.space2
            Layout.rightMargin: page.ui.space2
            spacing: page.ui.space2
            Item { Layout.preferredWidth: 30 }
            Label {
                text: page.ui.t("名称 / 来源", "Name / source")
                color: page.ui.muted
                font.pixelSize: page.ui.fontMicro
                font.weight: Font.Medium
                font.letterSpacing: 0.8
                Layout.fillWidth: true
            }
            Repeater {
                model: [page.ui.t("当前", "Current"), page.ui.t("最低", "Min"), page.ui.t("最高", "Max"), page.ui.t("平均", "Average")]
                delegate: Label {
                    required property string modelData
                    text: modelData
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontMicro
                    font.weight: Font.Medium
                    font.letterSpacing: 0.4
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: 76
                }
            }
        }
        ListView {
            id: sensorList
            objectName: "sensorList"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(520, Math.max(180, page.visibleReadings.length * 64))
            clip: true
            model: page.visibleReadings
            ScrollBar.vertical: ScrollBar {}
            delegate: Item {
                id: sensorRow
                required property var modelData
                width: sensorList.width
                height: 64
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: page.ui.radiusMd
                    color: rowHover.hovered ? page.ui.soft : "transparent"
                    Behavior on color { ColorAnimation { duration: page.ui.durationFast } }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: page.ui.space2
                    anchors.rightMargin: page.ui.space2
                    spacing: page.ui.space2
                    Button {
                        id: favoriteButton
                        implicitWidth: 30
                        implicitHeight: 30
                        checkable: true
                        checked: sensorRow.modelData.favorite
                        hoverEnabled: true
                        Accessible.name: (checked ? page.ui.t("取消关注 ", "Unfavorite ") : page.ui.t("关注 ", "Favorite ")) + sensorRow.modelData.name
                        onClicked: sensors.toggleFavorite(sensorRow.modelData.id)
                        contentItem: Icon {
                            name: "star"
                            color: sensorRow.modelData.favorite ? page.ui.accent : page.ui.muted
                            opacity: sensorRow.modelData.favorite ? 1 : 0.5
                            implicitWidth: 16
                            implicitHeight: 16
                        }
                        background: Rectangle {
                            radius: page.ui.radiusSm
                            color: favoriteButton.hovered ? page.ui.soft : "transparent"
                            border.width: favoriteButton.activeFocus ? page.ui.focusWidth : 0
                            border.color: page.ui.focusRing
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: page.ui.space1
                        Label {
                            text: sensorRow.modelData.name
                            color: page.ui.text
                            font.pixelSize: page.ui.fontSmall
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: sensorRow.modelData.group + " · " + sensorRow.modelData.source
                            color: page.ui.muted
                            font.pixelSize: page.ui.fontMicro
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                    Repeater {
                        model: [sensorRow.modelData.value, sensorRow.modelData.min, sensorRow.modelData.max, sensorRow.modelData.avg]
                        delegate: Label {
                            required property var modelData
                            required property int index
                            text: sensorRow.modelData.status === "available"
                                  ? page.ui.metric(modelData, 1, true) + (index === 0 ? " " + sensorRow.modelData.unit : "")
                                  : "—"
                            color: index === 0 ? page.ui.text : page.ui.secondary
                            font.pixelSize: page.ui.fontSmall
                            font.weight: index === 0 ? Font.DemiBold : Font.Normal
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 76
                            elide: Text.ElideRight
                        }
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: page.ui.borderWidth
                    color: page.ui.borderSubtle
                }
                HoverHandler { id: rowHover }
                ToolTip.visible: rowHover.hovered
                ToolTip.delay: 700
                ToolTip.text: page.ui.status(modelData.status) + "\n" + modelData.source + "\n" + (modelData.updatedAt || "")
            }
            EmptyState {
                anchors.centerIn: parent
                width: Math.min(parent.width - page.ui.space6, 300)
                visible: !page.visibleReadings.length
                ui: page.ui
                icon: sensors.readings.length ? "search" : "clock"
                title: sensors.readings.length ? page.ui.t("没有匹配的传感器", "No matching sensors") : page.ui.t("等待传感器采样…", "Waiting for samples…")
                message: sensors.readings.length ? page.ui.t("试试调整筛选条件。", "Try changing the filters.") : page.ui.t("本应用只显示实际可读取的系统指标。", "Only readable system metrics are shown.")
            }
        }
    }
}
