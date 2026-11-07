import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
    property string selectedId: ""
    property bool showSensitive: false
    property bool groupedDevices: true
    property var collapsedCategories: ({})
    readonly property var categories: [ui.t("全部类别", "All categories"), ...Array.from(new Set(hardware.devices.map(item => item.category)))]
    readonly property var filteredDevices: hardware.devices.filter(item =>
        (categoryBox.currentIndex <= 0 || item.category === categories[categoryBox.currentIndex]) &&
        (item.name + " " + item.category + " " + item.subtitle).toLowerCase().includes(search.text.toLowerCase()))
    readonly property var selectedDevice: filteredDevices.find(item => item.id === selectedId) || filteredDevices[0] || ({})
    readonly property var displayRows: {
        if (!groupedDevices) return filteredDevices.map(device => ({ kind: "device", device: device, showCategory: true }))
        const expand = search.text.length > 0
        const byCategory = new Map()
        for (const device of filteredDevices) {
            if (!byCategory.has(device.category)) byCategory.set(device.category, [])
            byCategory.get(device.category).push(device)
        }
        const rows = []
        for (const category of Array.from(byCategory.keys()).sort(page.compareCategories)) {
            const devices = byCategory.get(category)
            const collapsed = !expand && !!collapsedCategories[category]
            rows.push({ kind: "category", category: category, count: devices.length, collapsed: collapsed })
            if (!collapsed) for (const device of devices) rows.push({ kind: "device", device: device, showCategory: false })
        }
        return rows
    }
    readonly property int listHeight: displayRows.reduce((total, row) =>
        total + (row.kind === "category" ? 34 : 64) + page.ui.space1, 8)
    readonly property int collapsedCount: Object.keys(collapsedCategories).filter(key => collapsedCategories[key]).length
    function categoryRank(category) {
        const order = { "CPU": 0, "内存": 1, "主板/BIOS": 2, "存储": 3, "显卡": 4, "网络": 5,
                        "显示器": 6, "音频": 7, "电池": 8, "电源": 9, "PCI": 10, "USB": 11, "系统": 12 }
        return order[category] === undefined ? 99 : order[category]
    }
    function compareCategories(a, b) {
        return categoryRank(a) - categoryRank(b) || String(a).localeCompare(String(b))
    }
    function toggleCategory(category) {
        const next = Object.assign({}, collapsedCategories)
        next[category] = !next[category]
        collapsedCategories = next
    }
    function toggleAllCategories() {
        if (collapsedCount > 0) { collapsedCategories = {}; return }
        const next = {}
        for (const device of filteredDevices) next[device.category] = true
        collapsedCategories = next
    }
    function fieldText(field) {
        if (!field) return "—"
        if (field.value === undefined || field.value === null || String(field.value) === "")
            return page.ui.status(field.status)
        if (field.unit === "°C" && Number.isFinite(Number(field.value)))
            return page.ui.metric(page.ui.temperature(Number(field.value)), 1) + " " + page.ui.temperatureLabel()
        return String(field.value)
    }
    PageHeader {
        ui: page.ui
        title: page.ui.t("硬件信息", "Hardware inventory")
        description: page.ui.t("从设备到每个字段，查看规格、来源与读取状态。", "Explore devices, specifications, sources and read status.")
        eyebrow: page.ui.t("设备清单 · ", "DEVICE INVENTORY · ") + hardware.devices.length
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: page.ui.space3
        SearchBox {
            id: search
            ui: page.ui
            objectName: "hardwareSearch"
            placeholderText: page.ui.t("搜索设备名称…", "Search devices…")
            Layout.fillWidth: true
        }
        SelectBox {
            id: categoryBox
            ui: page.ui
            objectName: "hardwareCategoryFilter"
            model: page.categories.map(item => page.ui.category(item))
            Layout.preferredWidth: 185
        }
        CheckField {
            ui: page.ui
            text: page.ui.t("显示隐私字段", "Show private fields")
            checked: page.showSensitive
            onToggled: page.showSensitive = checked
        }
        CheckField {
            ui: page.ui
            text: page.ui.t("分组显示", "Grouped")
            checked: page.groupedDevices
            onToggled: page.groupedDevices = checked
            objectName: "groupedHardwareToggle"
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: page.ui.space5
        Layout.alignment: Qt.AlignTop
        Panel {
            ui: page.ui
            elevation: page.ui.elevationFlat
            padding: page.ui.space2
            Layout.preferredWidth: page.width > 1080 ? 270 : 226
            Layout.alignment: Qt.AlignTop
            spacing: page.ui.space2
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: page.ui.space2
                Layout.rightMargin: page.ui.space2
                Layout.topMargin: page.ui.space2
                Label {
                    text: page.ui.t("设备树", "DEVICES") + " · " + page.filteredDevices.length
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontMicro
                    font.weight: Font.Medium
                    font.letterSpacing: 1.2
                    Layout.fillWidth: true
                }
                ActionButton {
                    ui: page.ui
                    compact: true
                    visible: page.groupedDevices && page.filteredDevices.length > 0
                    text: page.collapsedCount > 0 ? page.ui.t("展开", "Expand") : page.ui.t("收起", "Collapse")
                    symbol: page.collapsedCount > 0 ? "chevronDown" : "chevronUp"
                    onClicked: page.toggleAllCategories()
                    objectName: "collapseAllHardwareButton"
                }
            }
            ListView {
                id: deviceList
                objectName: "deviceList"
                Layout.fillWidth: true
                Layout.preferredHeight: page.listHeight
                clip: true
                spacing: page.ui.space1
                model: page.displayRows
                ScrollBar.vertical: ScrollBar {}
                delegate: Item {
                    id: deviceDelegate
                    required property var modelData
                    width: deviceList.width
                    height: modelData.kind === "category" ? 34 : 64
                    Loader {
                        id: deviceLoader
                        anchors.fill: parent
                        property var rowData: deviceDelegate.modelData
                        sourceComponent: rowData.kind === "category" ? hardwareCategoryHeader : hardwareDeviceButton
                        Binding {
                            target: deviceLoader.item
                            property: "rowData"
                            value: deviceLoader.rowData
                            when: deviceLoader.item !== null
                        }
                    }
                }
                EmptyState {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - page.ui.space4, 240)
                    visible: !page.filteredDevices.length
                    ui: page.ui
                    icon: hardware.busy && !hardware.devices.length ? "clock" : "search"
                    title: hardware.busy && !hardware.devices.length ? page.ui.t("正在采集设备…", "Collecting devices…") : page.ui.t("没有匹配设备", "No matching devices")
                    message: hardware.busy && !hardware.devices.length ? page.ui.t("硬件清单来自本机实际采集。", "Inventory comes from this machine.") : page.ui.t("试试清空搜索或切换类别。", "Try clearing the search or changing the category.")
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: page.ui.space5
            Panel {
                ui: page.ui
                Layout.fillWidth: true
                spacing: page.ui.space2
                Label {
                    text: page.selectedDevice.category ? page.ui.category(page.selectedDevice.category) : page.ui.t("设备", "DEVICE")
                    color: page.ui.accent
                    font.pixelSize: page.ui.fontCaption
                    font.weight: Font.Medium
                    font.letterSpacing: 1.2
                }
                Label {
                    text: page.selectedDevice.name || page.ui.t("尚无可用设备", "No device available")
                    color: page.selectedDevice.name ? page.ui.text : page.ui.muted
                    font.pixelSize: page.ui.fontHeading
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                Label {
                    text: page.selectedDevice.subtitle || "—"
                    color: page.ui.secondary
                    font.pixelSize: page.ui.fontBody
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    Layout.topMargin: page.ui.space1
                    spacing: page.ui.space3
                    StatusChip {
                        ui: page.ui
                        visible: !!page.selectedDevice.status
                        text: page.ui.status(page.selectedDevice.status)
                        tone: page.selectedDevice.status === "available" ? page.ui.green : page.ui.warning
                    }
                    RowLayout {
                        spacing: page.ui.space1
                        Icon { name: "clock"; color: page.ui.muted; implicitWidth: 12; implicitHeight: 12 }
                        Label {
                            text: page.ui.t("采集时间：", "Collected: ") + (page.selectedDevice.collectedAt || hardware.lastUpdated || "—")
                            color: page.ui.muted
                            font.pixelSize: page.ui.fontMicro
                            elide: Text.ElideRight
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: sourceLabel.implicitHeight + page.ui.space2 * 2
                    radius: page.ui.radiusSm
                    color: page.ui.soft
                    Label {
                        id: sourceLabel
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: page.ui.space2 }
                        text: page.selectedDevice.source || page.ui.t("来源未提供", "No source reported")
                        color: page.ui.muted
                        font.pixelSize: page.ui.fontMicro
                        font.family: page.ui.monoFamily
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
            Panel {
                ui: page.ui
                Layout.fillWidth: true
                spacing: page.ui.space3
                SectionHeader {
                    ui: page.ui
                    title: page.ui.t("详细参数", "Specifications")
                    detail: page.ui.t("可选择并复制字段值", "Select values to copy")
                }
                EmptyState {
                    Layout.fillWidth: true
                    Layout.topMargin: page.ui.space3
                    Layout.bottomMargin: page.ui.space3
                    visible: !(page.selectedDevice.fields || []).length
                    ui: page.ui
                    icon: "hardware"
                    title: page.ui.t("暂无详细字段", "No detailed fields")
                    message: page.ui.t("系统暂未提供该设备的详细字段。", "The system has not exposed detailed fields for this device.")
                }
                Repeater {
                    model: page.selectedDevice.fields || []
                    delegate: ColumnLayout {
                        id: fieldRow
                        required property var modelData
                        readonly property string valueText: page.fieldText(modelData)
                        Layout.fillWidth: true
                        spacing: page.ui.space2
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: page.ui.space3
                            Label {
                                text: fieldRow.modelData.key
                                color: page.ui.secondary
                                font.pixelSize: page.ui.fontSmall
                                font.weight: Font.Medium
                                Layout.preferredWidth: Math.min(160, page.width * 0.17)
                                wrapMode: Text.WordWrap
                            }
                            TextEdit {
                                id: fieldValue
                                Layout.fillWidth: true
                                text: fieldRow.modelData.sensitive && !page.showSensitive ? page.ui.t("已隐藏 · 隐私字段", "Hidden · private field") : fieldRow.valueText
                                color: fieldRow.modelData.status === "available" ? page.ui.text : page.ui.warning
                                font.pixelSize: page.ui.fontSmall
                                readOnly: true
                                                    wrapMode: TextEdit.WrapAnywhere
                                selectionColor: page.ui.accent
                                selectedTextColor: page.ui.accentText
                                Accessible.name: fieldRow.modelData.key
                            }
                            ActionButton {
                                ui: page.ui
                                text: page.ui.t("复制", "Copy")
                                compact: true
                                enabled: !fieldRow.modelData.sensitive || page.showSensitive
                                onClicked: { fieldValue.selectAll(); fieldValue.copy(); fieldValue.deselect() }
                            }
                        }
                        Label {
                            readonly property bool showLine: preferences.showDiagnostics ||
                                                             fieldRow.modelData.status !== "available"
                            visible: showLine
                            text: {
                                const reason = fieldRow.modelData.reason ? page.ui.fieldReason(fieldRow.modelData.reason) : ""
                                return page.ui.status(fieldRow.modelData.status) + (reason ? " · " + reason : "")
                                        + " · " + (fieldRow.modelData.source || "—")
                            }
                            color: page.ui.muted
                            font.pixelSize: page.ui.fontMicro
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            implicitHeight: page.ui.borderWidth
                            color: page.ui.borderSubtle
                            Layout.fillWidth: true
                            Layout.topMargin: page.ui.space1
                        }
                    }
                }
            }
        }
    }
    Component {
        id: hardwareCategoryHeader
        Item {
            property var rowData
            Button {
                id: categoryButton
                anchors.fill: parent
                hoverEnabled: true
                Accessible.name: rowData ? page.ui.category(rowData.category) : ""
                onClicked: if (rowData) page.toggleCategory(rowData.category)
                background: Rectangle {
                    radius: page.ui.radiusSm
                    color: categoryButton.hovered ? page.ui.soft : "transparent"
                    Behavior on color { ColorAnimation { duration: page.ui.durationFast } }
                }
                contentItem: RowLayout {
                    spacing: page.ui.space1
                    Icon {
                        name: "chevronDown"
                        color: page.ui.muted
                        implicitWidth: 12
                        implicitHeight: 12
                        rotation: rowData && rowData.collapsed ? -90 : 0
                        Behavior on rotation { NumberAnimation { duration: page.ui.durationFast } }
                    }
                    Label {
                        text: rowData ? page.ui.category(rowData.category) : ""
                        color: page.ui.secondary
                        font.pixelSize: page.ui.fontMicro
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.8
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: rowData ? rowData.count : ""
                        color: page.ui.muted
                        font.pixelSize: page.ui.fontMicro
                    }
                }
            }
        }
    }
    Component {
        id: hardwareDeviceButton
        Item {
            property var rowData
            readonly property var device: rowData ? rowData.device : ({})
            Button {
                id: deviceButton
                anchors.fill: parent
                leftPadding: page.ui.space3
                rightPadding: page.ui.space3
                hoverEnabled: true
                Accessible.name: device.name || ""
                onClicked: page.selectedId = device.id
                contentItem: Column {
                    spacing: page.ui.space1
                    Label {
                        visible: rowData && rowData.showCategory
                        text: page.ui.category(device.category)
                        color: page.selectedDevice.id === device.id ? page.ui.accent : page.ui.muted
                        font.pixelSize: page.ui.fontMicro
                        font.weight: Font.Medium
                        font.letterSpacing: 0.4
                    }
                    Label {
                        width: parent.width
                        text: device.name || ""
                        elide: Text.ElideRight
                        color: page.ui.text
                        font.pixelSize: page.ui.fontSmall
                        font.weight: page.selectedDevice.id === device.id ? Font.Medium : Font.Normal
                    }
                }
                background: Rectangle {
                    radius: page.ui.radiusMd
                    color: page.selectedDevice.id === device.id ? page.ui.accentSoft
                         : deviceButton.hovered ? page.ui.soft : "transparent"
                    border.width: deviceButton.activeFocus ? page.ui.focusWidth : 0
                    border.color: page.ui.focusRing
                    Behavior on color { ColorAnimation { duration: page.ui.durationFast } }
                    Rectangle {
                        x: 0
                        anchors.verticalCenter: parent.verticalCenter
                        width: 3
                        height: 22
                        radius: page.ui.radiusPill
                        color: page.ui.accent
                        opacity: page.selectedDevice.id === device.id ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: page.ui.durationFast } }
                    }
                }
            }
        }
    }

}
