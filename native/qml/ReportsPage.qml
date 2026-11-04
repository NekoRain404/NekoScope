pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

PageView {
    id: page
    property var selectedCategories: []
    property string previewText: ""
    property bool initialized: false
    readonly property var categories: Array.from(new Set(hardware.devices.map(item => item.category)))
    readonly property string format: ["html", "json", "csv"][formatBox.currentIndex]
    function selectAll() { selectedCategories = categories.slice(); refreshPreview() }
    function toggleCategory(category, included) {
        selectedCategories = included ? [...selectedCategories, category] : selectedCategories.filter(item => item !== category)
        refreshPreview()
    }
    function refreshPreview() {
        if (!initialized) return
        previewText = selectedCategories.length ? reports.preview(format, includeSensitive.checked, selectedCategories) : ""
    }
    onVisibleChanged: if (visible && initialized) { if (!selectedCategories.length) selectAll(); else refreshPreview() }
    onCategoriesChanged: if (initialized && !selectedCategories.length) selectAll()
    Component.onCompleted: { initialized = true; selectAll() }
    FileDialog {
        id: saveDialog
        objectName: "reportFileDialog"
        title: page.ui.t("导出系统报告", "Export system report")
        fileMode: FileDialog.SaveFile
        defaultSuffix: page.format
        nameFilters: page.format === "html" ? ["HTML (*.html)"] : page.format === "json" ? ["JSON (*.json)"] : ["CSV (*.csv)"]
        onAccepted: reports.save(selectedFile, page.format, includeSensitive.checked, page.selectedCategories)
    }
    PageHeader {
        ui: page.ui; title: page.ui.t("系统报告", "A report worth sharing")
        description: page.ui.t("选择内容，检查脱敏预览，然后保存到你指定的位置。", "Choose the content, review privacy, then save it where you want.")
        eyebrow: page.ui.t("导出 · 由你掌控", "EXPORT · YOUR DATA, YOUR CHOICE")
        ActionButton { ui: page.ui; text: page.ui.t("导出报告", "Export report"); primary: true; symbol: "report"; enabled: page.selectedCategories.length > 0; onClicked: saveDialog.open(); objectName: "exportReportButton" }
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 18; Layout.alignment: Qt.AlignTop
        Panel {
            ui: page.ui; Layout.preferredWidth: page.width > 1100 ? 305 : 265; Layout.alignment: Qt.AlignTop; spacing: 15
            RowLayout {
                Layout.fillWidth: true
                Label { text: page.ui.t("包含的硬件", "Include hardware"); color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                Item { Layout.fillWidth: true }
                ActionButton { ui: page.ui; text: page.ui.t("全选", "All"); compact: true; onClicked: page.selectAll() }
            }
            Repeater {
                model: page.categories
                delegate: CheckBox {
                    required property string modelData
                    text: page.ui.category(modelData)
                    checked: page.selectedCategories.includes(modelData)
                    onToggled: page.toggleCategory(modelData, checked)
                    font.pixelSize: 12; Layout.fillWidth: true
                    objectName: "reportCategory_" + modelData
                }
            }
            Label { visible: !page.categories.length; text: page.ui.t("等待硬件清单…", "Waiting for inventory…"); color: page.ui.muted; font.pixelSize: 11 }
            Rectangle { height: 1; color: page.ui.border; Layout.fillWidth: true; Layout.topMargin: 4; Layout.bottomMargin: 4 }
            Label { text: page.ui.t("隐私控制", "Privacy"); color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
            CheckBox { id: includeSensitive; text: page.ui.t("包含可识别设备的信息", "Include identifying information"); checked: false; font.pixelSize: 11; Layout.fillWidth: true; onToggled: page.refreshPreview(); objectName: "includeSensitiveCheckBox" }
            Label { text: includeSensitive.checked ? page.ui.t("当前报告将包含主机名、序列号和网络地址等隐私字段。", "This report will include private fields such as hostnames, serial numbers and network addresses.") : page.ui.t("默认脱敏主机名、序列号、MAC、地址与用户路径等可识别信息。", "Hostnames, serial numbers, MAC addresses, network addresses and user paths are redacted by default."); color: includeSensitive.checked ? page.ui.warning : page.ui.muted; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Rectangle { height: 1; color: page.ui.border; Layout.fillWidth: true; Layout.topMargin: 4; Layout.bottomMargin: 4 }
            Label { text: page.ui.t("导出格式", "Export format"); color: page.ui.secondary; font.pixelSize: 11 }
            ComboBox { id: formatBox; model: [page.ui.t("HTML · 可读报告", "HTML · Readable report"), page.ui.t("JSON · 结构化数据", "JSON · Structured data"), page.ui.t("CSV · 字段表格", "CSV · Field table")]; Layout.fillWidth: true; onCurrentIndexChanged: page.refreshPreview(); objectName: "reportFormatSelector" }
            ActionButton { ui: page.ui; text: page.ui.t("刷新预览", "Refresh preview"); symbol: "refresh"; Layout.fillWidth: true; enabled: page.selectedCategories.length > 0; onClicked: page.refreshPreview(); objectName: "refreshReportPreviewButton" }
        }
        Panel {
            ui: page.ui; Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
            RowLayout {
                Layout.fillWidth: true
                Label { text: page.ui.t("导出预览", "Export preview"); color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                Item { Layout.fillWidth: true }
                Label { text: includeSensitive.checked ? page.ui.t("包含隐私字段", "Private fields included") : page.ui.t("隐私脱敏已开启", "Privacy redaction on"); color: includeSensitive.checked ? page.ui.warning : page.ui.green; font.pixelSize: 10 }
            }
            Label { text: page.ui.t("预览与导出采用相同的类别、格式和隐私选项。", "Preview and export use the same category, format and privacy options."); color: page.ui.muted; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ScrollView {
                Layout.fillWidth: true; Layout.preferredHeight: 595; clip: true
                TextArea {
                    id: preview
                    objectName: "reportPreview"
                    text: page.previewText || page.ui.t("选择至少一个硬件类别以生成报告预览。", "Choose at least one hardware category to preview a report.")
                    textFormat: page.format === "html" ? TextEdit.RichText : TextEdit.PlainText
                    readOnly: true; selectByMouse: true
                    color: page.format === "html" ? "#292b39" : page.ui.text; font.family: page.format === "html" ? "Noto Sans" : "monospace"; font.pixelSize: 11
                    wrapMode: TextEdit.WrapAnywhere
                    background: Rectangle { color: page.format === "html" ? "#ffffff" : page.ui.soft; radius: 8 }
                    padding: 18
                }
            }
            Label { visible: !!reports.error; text: reports.error; color: page.ui.warning; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { visible: !!reports.lastPath; text: page.ui.t("已保存至：", "Saved to: ") + reports.lastPath; color: page.ui.green; font.pixelSize: 10; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; objectName: "reportSavedPathLabel" }
        }
    }
}
