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
    PageHeader {
        ui: page.ui; title: page.ui.t("硬件信息", "Hardware inventory")
        description: page.ui.t("从设备到每个字段，查看规格、来源与读取状态。", "Explore devices, specifications, sources and read status.")
        eyebrow: page.ui.t("设备清单 · ", "DEVICE INVENTORY · ") + hardware.devices.length
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 12
        TextField { id: search; objectName: "hardwareSearch"; placeholderText: page.ui.t("搜索设备名称…", "Search devices…"); Layout.fillWidth: true; implicitHeight: 40; leftPadding: 13; selectByMouse: true }
        ComboBox { id: categoryBox; objectName: "hardwareCategoryFilter"; model: page.categories.map(item => page.ui.category(item)); Layout.preferredWidth: 185; implicitHeight: 40 }
        CheckBox { text: page.ui.t("显示隐私字段", "Show private fields"); checked: page.showSensitive; onToggled: page.showSensitive = checked; font.pixelSize: 11 }
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 18; Layout.alignment: Qt.AlignTop
        Panel {
            ui: page.ui; padding: 12; Layout.preferredWidth: page.width > 1080 ? 260 : 218; Layout.alignment: Qt.AlignTop
            Label { text: page.ui.t("设备树", "DEVICES") + " · " + page.filteredDevices.length; color: page.ui.muted; font.pixelSize: 10; font.letterSpacing: 1; Layout.margins: 8 }
            ListView {
                id: deviceList
                objectName: "deviceList"
                Layout.fillWidth: true; Layout.preferredHeight: 540; clip: true; spacing: 4
                model: page.filteredDevices
                ScrollBar.vertical: ScrollBar {}
                delegate: Button {
                    id: deviceButton
                    required property var modelData
                    width: deviceList.width; implicitHeight: 67; leftPadding: 12; rightPadding: 12; hoverEnabled: true
                    Accessible.name: modelData.name
                    onClicked: page.selectedId = modelData.id
                    contentItem: Column {
                        spacing: 5
                        Label { text: page.ui.category(deviceButton.modelData.category); color: page.selectedDevice.id === deviceButton.modelData.id ? page.ui.accent : page.ui.muted; font.pixelSize: 10 }
                        Label { width: parent.width; text: deviceButton.modelData.name; elide: Text.ElideRight; color: page.ui.text; font.pixelSize: 11 }
                    }
                    background: Rectangle { radius: 8; color: page.selectedDevice.id === deviceButton.modelData.id ? page.ui.accentSoft : deviceButton.hovered ? page.ui.soft : "transparent"; border.color: deviceButton.activeFocus ? page.ui.accent : "transparent" }
                }
                Label { anchors.centerIn: parent; visible: !page.filteredDevices.length; text: page.ui.t("没有匹配设备", "No matching devices"); color: page.ui.muted; font.pixelSize: 11 }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.alignment: Qt.AlignTop; spacing: 18
            Panel {
                ui: page.ui; Layout.fillWidth: true; spacing: 10
                Label { text: page.ui.category(page.selectedDevice.category || ""); color: page.ui.accent; font.pixelSize: 10 }
                Label { text: page.selectedDevice.name || page.ui.t("尚无可用设备", "No device available"); color: page.ui.text; font.pixelSize: 21; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: page.selectedDevice.subtitle || "—"; color: page.ui.secondary; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: page.ui.status(page.selectedDevice.status); color: page.selectedDevice.status === "available" ? page.ui.green : page.ui.warning; font.pixelSize: 10 }
                Label { text: page.selectedDevice.source || ""; color: page.ui.muted; font.pixelSize: 10; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                Label { text: page.ui.t("采集时间：", "Collected: ") + (page.selectedDevice.collectedAt || hardware.lastUpdated || "—"); color: page.ui.muted; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
            Panel {
                ui: page.ui; Layout.fillWidth: true; spacing: 16
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: page.ui.t("详细参数", "Specifications"); color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }
                    Label { text: page.ui.t("可选择并复制字段值", "Select values to copy"); color: page.ui.muted; font.pixelSize: 9 }
                }
                Repeater {
                    model: page.selectedDevice.fields || []
                    delegate: ColumnLayout {
                        id: fieldRow
                        required property var modelData
                        Layout.fillWidth: true; spacing: 7
                        RowLayout {
                            Layout.fillWidth: true; spacing: 12
                            Label { text: fieldRow.modelData.key; color: page.ui.secondary; font.pixelSize: 11; Layout.preferredWidth: Math.min(150, page.width * 0.16); wrapMode: Text.WordWrap }
                            TextEdit {
                                id: fieldValue
                                Layout.fillWidth: true
                                text: fieldRow.modelData.sensitive && !page.showSensitive ? page.ui.t("已隐藏 · 隐私字段", "Hidden · private field") : (String(fieldRow.modelData.value || "") || page.ui.status(fieldRow.modelData.status))
                                color: fieldRow.modelData.status === "available" ? page.ui.text : page.ui.warning
                                font.pixelSize: 11; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere
                                selectionColor: page.ui.accent; selectedTextColor: "white"
                                Accessible.name: fieldRow.modelData.key
                            }
                            ActionButton { ui: page.ui; text: page.ui.t("复制", "Copy"); compact: true; enabled: !fieldRow.modelData.sensitive || page.showSensitive; onClicked: { fieldValue.selectAll(); fieldValue.copy(); fieldValue.deselect() } }
                        }
                        Label { text: page.ui.status(fieldRow.modelData.status) + " · " + (fieldRow.modelData.source || "—"); color: page.ui.muted; font.pixelSize: 9; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                        Rectangle { height: 1; color: page.ui.border; Layout.fillWidth: true; Layout.topMargin: 5 }
                    }
                }
                Label { visible: !(page.selectedDevice.fields || []).length; text: page.ui.t("系统暂未提供该设备的详细字段。", "The system has not exposed detailed fields for this device."); color: page.ui.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
    }
}
