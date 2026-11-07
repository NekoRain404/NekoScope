pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
    property bool grouped: true
    property var collapsedDevices: ({})

    readonly property var deviceIds: {
        const ids = []
        for (const item of sensors.readings) {
            const id = item.deviceId || "system"
            if (!ids.includes(id)) ids.push(id)
        }
        return ids.sort(page.compareDevices)
    }
    readonly property var deviceChoices: [page.ui.t("全部设备", "All devices"),
        ...deviceIds.map(id => page.deviceDisplay(id))]
    readonly property var filtered: sensors.readings.filter(item => {
        if (favoritesOnly.checked && !item.favorite) return false
        const id = item.deviceId || "system"
        if (deviceBox.currentIndex > 0 && id !== deviceIds[deviceBox.currentIndex - 1]) return false
        const haystack = (item.name + " " + item.group + " " + item.unit + " " + item.source + " "
                          + (item.device || "")).toLowerCase()
        return haystack.includes(search.text.toLowerCase())
    })
    readonly property var displayRows: {
        if (!grouped) return filtered.map(item => ({ kind: "sensor", reading: item, showCategory: true }))
        const byDevice = new Map()
        for (const item of filtered) {
            const id = item.deviceId || "system"
            if (!byDevice.has(id)) byDevice.set(id, [])
            byDevice.get(id).push(item)
        }
        const rows = []
        for (const id of Array.from(byDevice.keys()).sort(page.compareDevices)) {
            const items = byDevice.get(id)
            const collapsed = !!collapsedDevices[id]
            rows.push({ kind: "device", deviceId: id, deviceName: page.deviceDisplay(id),
                        count: items.length, collapsed: collapsed })
            if (collapsed) continue
            const categories = []
            for (const item of items) if (!categories.includes(item.group)) categories.push(item.group)
            categories.sort(page.compareCategories)
            for (const category of categories) {
                const categoryItems = items.filter(item => item.group === category)
                rows.push({ kind: "category", category: category, count: categoryItems.length })
                for (const item of categoryItems)
                    rows.push({ kind: "sensor", reading: item, showCategory: false })
            }
        }
        return rows
    }
    readonly property int listHeight: displayRows.reduce((total, row) =>
        total + (row.kind === "device" ? 44 : row.kind === "category" ? 30 : 64), 0)
    readonly property int collapsedCount: deviceIds.filter(id => !!collapsedDevices[id]).length

    function deviceRank(id) {
        const order = { "cpu": 0, "gpu": 1, "nvme": 2, "storage": 3, "memory": 4,
                        "battery": 5, "network": 6, "motherboard": 7, "system": 8 }
        return order[id] === undefined ? 99 : order[id]
    }
    function compareDevices(a, b) {
        return deviceRank(a) - deviceRank(b) || page.deviceDisplay(a).localeCompare(page.deviceDisplay(b))
    }
    function compareCategories(a, b) {
        const order = { "Frequency": 0, "Temperature": 1, "Cooling": 2, "Voltage": 3, "Power": 4,
                        "CPU": 5, "CPU cores": 6, "GPU": 7, "GPU memory": 8, "Memory": 9,
                        "Battery": 10, "Disk I/O": 11, "Network": 12, "System": 13 }
        const ai = order[a] === undefined ? 99 : order[a]
        const bi = order[b] === undefined ? 99 : order[b]
        return ai - bi || String(a).localeCompare(String(b))
    }
    function displayMetric(reading, value) {
        if (reading.unit === "MHz") {
            const number = Number(value)
            return Number.isFinite(number) ? Math.round(number) : NaN
        }
        return reading.unit === "°C" ? page.ui.temperature(value) : value
    }
    function displayDigits(reading) {
        return reading.unit === "MHz" ? 0 : 1
    }
    function displayUnit(reading) {
        return reading.unit === "°C" ? page.ui.temperatureLabel() : reading.unit
    }
    function deviceDisplay(id) {
        const item = sensors.readings.find(reading => (reading.deviceId || "system") === id)
        return page.ui.deviceName(id, item ? item.device : id)
    }
    function toggleDevice(id) {
        const next = Object.assign({}, collapsedDevices)
        next[id] = !next[id]
        collapsedDevices = next
    }
    function toggleAll() {
        if (collapsedCount > 0) { collapsedDevices = {}; return }
        const next = {}
        for (const id of deviceIds) next[id] = true
        collapsedDevices = next
    }

    PageHeader {
        ui: page.ui
        title: page.ui.t("实时传感器", "Follow every signal")
        description: page.ui.t("按设备分组关注温度与负载，保留当前会话的变化轨迹。", "Grouped by device, tracking every signal throughout this session.")
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
    GridLayout {
        Layout.fillWidth: true
        columns: page.width > 1150 ? 4 : 2
        columnSpacing: page.ui.space4
        rowSpacing: page.ui.space4
        MetricCard {
            ui: page.ui
            Layout.fillWidth: true
            title: page.ui.t("CPU 使用率", "CPU usage")
            value: page.ui.metric(sensors.summary.cpuUsage, 1)
            unit: "%"
            detail: page.ui.t("从 /proc/stat 计算", "Calculated from /proc/stat")
        }
        MetricCard {
            ui: page.ui
            Layout.fillWidth: true
            title: page.ui.t("CPU 频率", "CPU frequency")
            value: page.ui.metric(sensors.summary.cpuFrequencyGHz, 2)
            unit: "GHz"
            detail: page.ui.t("最高 ", "Max ") + page.ui.metric(sensors.summary.cpuFrequencyMaxGHz, 2)
                    + " · " + page.ui.t("最低 ", "Min ") + page.ui.metric(sensors.summary.cpuFrequencyMinGHz, 2) + " GHz"
            tone: page.ui.accent
        }
        MetricCard {
            ui: page.ui
            Layout.fillWidth: true
            title: page.ui.t("CPU 温度", "CPU temperature")
            value: page.ui.metric(page.ui.temperature(sensors.summary.cpuTemperature), 1)
            unit: page.ui.temperatureLabel()
            detail: page.ui.t("无可用通道时显示 —", "— indicates no readable channel")
            tone: page.ui.warning
        }
        MetricCard {
            ui: page.ui
            Layout.fillWidth: true
            title: page.ui.t("内存使用率", "Memory usage")
            value: page.ui.metric(sensors.summary.memoryUsage, 1)
            unit: "%"
            detail: page.ui.metric(sensors.summary.memoryUsedGiB, 1) + " / " + page.ui.metric(sensors.summary.memoryTotalGiB, 1) + " GiB"
            tone: page.ui.green
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
            samples: seriesBox.currentIndex === 0
                     ? sensors.cpuHistory
                     : sensors.temperatureHistory.map(value => page.ui.temperature(value))
            lineColor: seriesBox.currentIndex === 0 ? page.ui.accent : page.ui.warning
            maximum: seriesBox.currentIndex === 0 ? 100 : page.ui.temperatureMax()
            unit: seriesBox.currentIndex === 0 ? "%" : page.ui.temperatureLabel()
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
            detail: filtered.length + page.ui.t(" 个通道", " channels")
            ActionButton {
                ui: page.ui
                compact: true
                visible: page.grouped && page.deviceIds.length > 0
                text: page.collapsedCount > 0 ? page.ui.t("全部展开", "Expand all") : page.ui.t("全部收起", "Collapse all")
                symbol: page.collapsedCount > 0 ? "chevronDown" : "chevronUp"
                onClicked: page.toggleAll()
                objectName: "collapseAllSensorsButton"
            }
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
            CheckField {
                id: groupedOnly
                ui: page.ui
                text: page.ui.t("分组显示", "Grouped")
                checked: page.grouped
                onToggled: page.grouped = checked
                objectName: "groupedSensorsToggle"
            }
            Item { Layout.fillWidth: true }
            SelectBox {
                id: deviceBox
                ui: page.ui
                model: page.deviceChoices
                Layout.preferredWidth: 180
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
            Layout.preferredHeight: Math.min(640, Math.max(220, page.listHeight))
            clip: true
            model: page.displayRows
            spacing: 0
            cacheBuffer: 600
            ScrollBar.vertical: ScrollBar {}
            delegate: Item {
                id: sensorDelegate
                required property var modelData
                width: sensorList.width
                height: modelData.kind === "device" ? 44 : modelData.kind === "category" ? 30 : 64
                Loader {
                    id: rowLoader
                    anchors.fill: parent
                    property var rowData: sensorDelegate.modelData
                    sourceComponent: rowData.kind === "device" ? deviceHeader
                                   : rowData.kind === "category" ? categoryHeader : sensorRow
                    Binding {
                        target: rowLoader.item
                        property: "rowData"
                        value: rowLoader.rowData
                        when: rowLoader.item !== null
                    }
                }
            }
            EmptyState {
                anchors.centerIn: parent
                width: Math.min(parent.width - page.ui.space6, 300)
                visible: !page.displayRows.length
                ui: page.ui
                icon: sensors.readings.length ? "search" : "clock"
                title: sensors.readings.length ? page.ui.t("没有匹配的传感器", "No matching sensors") : page.ui.t("等待传感器采样…", "Waiting for samples…")
                message: sensors.readings.length ? page.ui.t("试试调整筛选条件。", "Try changing the filters.") : page.ui.t("本应用只显示实际可读取的系统指标。", "Only readable system metrics are shown.")
            }
        }
    }

    Component {
        id: deviceHeader
        Item {
            property var rowData
            Button {
                id: deviceHeaderButton
                anchors.fill: parent
                hoverEnabled: true
                Accessible.name: rowData ? rowData.deviceName : ""
                onClicked: if (rowData) page.toggleDevice(rowData.deviceId)
                background: Rectangle {
                    radius: page.ui.radiusMd
                    color: deviceHeaderButton.hovered ? page.ui.soft : "transparent"
                    Behavior on color { ColorAnimation { duration: page.ui.durationFast } }
                }
                contentItem: RowLayout {
                    spacing: page.ui.space2
                    Item { Layout.preferredWidth: page.ui.space1 }
                    Icon {
                        name: "chevronDown"
                        color: page.ui.muted
                        implicitWidth: 14
                        implicitHeight: 14
                        rotation: rowData && rowData.collapsed ? -90 : 0
                        Behavior on rotation { NumberAnimation { duration: page.ui.durationFast } }
                    }
                    Icon {
                        name: rowData ? page.ui.deviceIcon(rowData.deviceId) : "sensors"
                        color: page.ui.accent
                        implicitWidth: 18
                        implicitHeight: 18
                    }
                    Label {
                        text: rowData ? rowData.deviceName : ""
                        color: page.ui.text
                        font.pixelSize: page.ui.fontBodyLarge
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: rowData ? rowData.count + page.ui.t(" 个通道", " channels") : ""
                        color: page.ui.muted
                        font.pixelSize: page.ui.fontMicro
                    }
                    Item { Layout.preferredWidth: page.ui.space2 }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: page.ui.borderWidth
                color: page.ui.borderSubtle
            }
        }
    }
    Component {
        id: categoryHeader
        Item {
            property var rowData
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: page.ui.space7
                anchors.rightMargin: page.ui.space3
                spacing: page.ui.space2
                Label {
                    text: rowData ? page.ui.sensorCategory(rowData.category) : ""
                    color: page.ui.secondary
                    font.pixelSize: page.ui.fontMicro
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.9
                    Layout.fillWidth: true
                }
                Label {
                    text: rowData ? rowData.count : ""
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontMicro
                }
            }
        }
    }
    Component {
        id: sensorRow
        Item {
            property var rowData
            readonly property var reading: rowData ? rowData.reading : ({})
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
                    checked: reading.favorite || false
                    hoverEnabled: true
                    Accessible.name: (checked ? page.ui.t("取消关注 ", "Unfavorite ") : page.ui.t("关注 ", "Favorite ")) + reading.name
                    onClicked: sensors.toggleFavorite(reading.id)
                    contentItem: Icon {
                        name: "star"
                        color: reading.favorite ? page.ui.accent : page.ui.muted
                        opacity: reading.favorite ? 1 : 0.5
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
                        text: reading.name || ""
                        color: page.ui.text
                        font.pixelSize: page.ui.fontSmall
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: rowData && rowData.showCategory
                              ? page.ui.sensorCategory(reading.group) + " · " + (reading.source || "")
                              : (reading.source || "")
                        color: page.ui.muted
                        font.pixelSize: page.ui.fontMicro
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
                Repeater {
                    model: [reading.value, reading.min, reading.max, reading.avg]
                    delegate: Label {
                        required property var modelData
                        required property int index
                        text: reading.status === "available"
                              ? page.ui.metric(page.displayMetric(reading, modelData), page.displayDigits(reading), true)
                                + (index === 0 ? " " + page.displayUnit(reading) : "")
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
            ToolTip.text: page.ui.status(reading.status) + "\n" + reading.source + "\n" + (reading.updatedAt || "")
        }
    }
}
