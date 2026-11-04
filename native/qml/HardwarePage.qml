pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
    property string selectedId: ""
    property bool showSensitive: false
    readonly property var categories: [ui.t("全部类别", "All categories"), ...Array.from(new Set(hardware.devices.map(item => item.category)))]
    readonly property var filteredDevices: hardware.devices.filter(item =>
        (categoryBox.currentIndex <= 0 || item.category === categories[categoryBox.currentIndex]) &&
        (item.name + " " + item.category + " " + item.subtitle).toLowerCase().includes(search.text.toLowerCase()))
    readonly property var selectedDevice: filteredDevices.find(item => item.id === selectedId) || filteredDevices[0] || ({})
    readonly property int listHeight: Math.min(620, Math.max(260, 44 + filteredDevices.length * 68))
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
            Label {
                text: page.ui.t("设备树", "DEVICES") + " · " + page.filteredDevices.length
                color: page.ui.muted
                font.pixelSize: page.ui.fontMicro
                font.weight: Font.Medium
                font.letterSpacing: 1.2
                Layout.leftMargin: page.ui.space2
                Layout.topMargin: page.ui.space2
            }
            ListView {
                id: deviceList
                objectName: "deviceList"
                Layout.fillWidth: true
                Layout.preferredHeight: page.listHeight
                clip: true
                spacing: page.ui.space1
                model: page.filteredDevices
                ScrollBar.vertical: ScrollBar {}
                delegate: Button {
                    id: deviceButton
                    required property var modelData
                    width: deviceList.width
                    implicitHeight: 64
                    leftPadding: page.ui.space3
                    rightPadding: page.ui.space3
                    hoverEnabled: true
                    Accessible.name: modelData.name
                    onClicked: page.selectedId = modelData.id
                    contentItem: Column {
                        spacing: page.ui.space1
                        Label {
                            text: page.ui.category(deviceButton.modelData.category)
                            color: page.selectedDevice.id === deviceButton.modelData.id ? page.ui.accent : page.ui.muted
                            font.pixelSize: page.ui.fontMicro
                            font.weight: Font.Medium
                            font.letterSpacing: 0.4
                        }
                        Label {
                            width: parent.width
                            text: deviceButton.modelData.name
                            elide: Text.ElideRight
                            color: page.ui.text
                            font.pixelSize: page.ui.fontSmall
                            font.weight: page.selectedDevice.id === deviceButton.modelData.id ? Font.Medium : Font.Normal
                        }
                    }
                    background: Rectangle {
                        radius: page.ui.radiusMd
                        color: page.selectedDevice.id === deviceButton.modelData.id ? page.ui.accentSoft
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
                            opacity: page.selectedDevice.id === deviceButton.modelData.id ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: page.ui.durationFast } }
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
                        readonly property string valueText: modelData.value === undefined || modelData.value === null || String(modelData.value) === ""
                                                                ? page.ui.status(modelData.status) : String(modelData.value)
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
                            text: page.ui.status(fieldRow.modelData.status) + " · " + (fieldRow.modelData.source || "—")
                            color: page.ui.muted
                            font.pixelSize: page.ui.fontMicro
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            height: page.ui.borderWidth
                            color: page.ui.borderSubtle
                            Layout.fillWidth: true
                            Layout.topMargin: page.ui.space1
                        }
                    }
                }
            }
        }
    }
}
