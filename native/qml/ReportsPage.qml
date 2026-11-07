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
    readonly property var formatIds: Array.from(reports.formats)
    readonly property string format: page.formatIds[formatBox.currentIndex] || ""
    readonly property var formatLabels: ({
        "html": ["HTML · 可读报告", "HTML · Readable report"],
        "json": ["JSON · 结构化数据", "JSON · Structured data"],
        "csv": ["CSV · 字段表格", "CSV · Field table"],
        "markdown": ["Markdown · 文档", "Markdown · Document"]
    })
    function formatLabel(id) {
        const entry = page.formatLabels[id]
        return entry ? page.ui.t(entry[0], entry[1]) : id
    }
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
        defaultSuffix: page.format === "markdown" ? "md" : page.format
        nameFilters: page.format === "html" ? ["HTML (*.html)"]
                   : page.format === "json" ? ["JSON (*.json)"]
                   : page.format === "csv" ? ["CSV (*.csv)"]
                   : ["Markdown (*.md)"]
        onAccepted: reports.save(selectedFile, page.format, includeSensitive.checked, page.selectedCategories)
    }
    PageHeader {
        ui: page.ui
        title: page.ui.t("系统报告", "A report worth sharing")
        description: page.ui.t("选择内容，检查脱敏预览，然后保存到你指定的位置。", "Choose the content, review privacy, then save it where you want.")
        eyebrow: page.ui.t("导出 · 由你掌控", "EXPORT · YOUR DATA, YOUR CHOICE")
        ActionButton {
            ui: page.ui
            text: page.ui.t("导出报告", "Export report")
            primary: true
            symbol: "report"
            enabled: page.selectedCategories.length > 0
            onClicked: saveDialog.open()
            objectName: "exportReportButton"
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: page.ui.space5
        Layout.alignment: Qt.AlignTop
        Panel {
            ui: page.ui
            Layout.preferredWidth: page.width > 1100 ? 310 : 270
            Layout.alignment: Qt.AlignTop
            spacing: page.ui.space3
            SectionHeader {
                ui: page.ui
                title: page.ui.t("包含的硬件", "Include hardware")
                ActionButton {
                    ui: page.ui
                    text: page.ui.t("全选", "All")
                    compact: true
                    onClicked: page.selectAll()
                }
            }
            EmptyState {
                Layout.fillWidth: true
                Layout.topMargin: page.ui.space2
                Layout.bottomMargin: page.ui.space2
                visible: !page.categories.length
                ui: page.ui
                icon: hardware.busy ? "clock" : "hardware"
                title: hardware.busy ? page.ui.t("正在采集硬件…", "Collecting hardware…") : page.ui.t("尚无硬件类别", "No hardware categories")
                message: page.ui.t("报告只包含本机实际采集到的设备。", "Reports contain only devices collected from this machine.")
            }
            Repeater {
                model: page.categories
                delegate: CheckField {
                    required property string modelData
                    ui: page.ui
                    text: page.ui.category(modelData)
                    checked: page.selectedCategories.includes(modelData)
                    onToggled: page.toggleCategory(modelData, checked)
                    Layout.fillWidth: true
                    objectName: "reportCategory_" + modelData
                }
            }
            Rectangle {
                height: page.ui.borderWidth
                color: page.ui.borderSubtle
                Layout.fillWidth: true
                Layout.topMargin: page.ui.space1
                Layout.bottomMargin: page.ui.space1
            }
            Label {
                text: page.ui.t("隐私控制", "Privacy")
                color: page.ui.text
                font.pixelSize: page.ui.fontSubtitle
                font.weight: Font.DemiBold
            }
            CheckField {
                id: includeSensitive
                ui: page.ui
                text: page.ui.t("包含可识别设备的信息", "Include identifying information")
                checked: false
                Layout.fillWidth: true
                onToggled: page.refreshPreview()
                objectName: "includeSensitiveCheckBox"
            }
            NoticeBanner {
                ui: page.ui
                Layout.fillWidth: true
                compact: true
                icon: includeSensitive.checked ? "warning" : "info"
                text: includeSensitive.checked ? page.ui.t("当前报告将包含主机名、序列号和网络地址等隐私字段。", "This report will include private fields such as hostnames, serial numbers and network addresses.") : page.ui.t("默认脱敏主机名、序列号、MAC、地址与用户路径等可识别信息。", "Hostnames, serial numbers, MAC addresses, network addresses and user paths are redacted by default.")
                tone: includeSensitive.checked ? page.ui.warning : page.ui.green
                surfaceColor: includeSensitive.checked ? page.ui.warningSoft : page.ui.positiveSoft
            }
            Rectangle {
                height: page.ui.borderWidth
                color: page.ui.borderSubtle
                Layout.fillWidth: true
                Layout.topMargin: page.ui.space1
                Layout.bottomMargin: page.ui.space1
            }
            Label {
                text: page.ui.t("导出格式", "Export format")
                color: page.ui.secondary
                font.pixelSize: page.ui.fontSmall
                font.weight: Font.Medium
            }
            SelectBox {
                id: formatBox
                ui: page.ui
                model: page.formatIds.map(item => page.formatLabel(item))
                Layout.fillWidth: true
                onCurrentIndexChanged: page.refreshPreview()
                objectName: "reportFormatSelector"
            }
            ActionButton {
                ui: page.ui
                text: page.ui.t("刷新预览", "Refresh preview")
                symbol: "refresh"
                Layout.fillWidth: true
                enabled: page.selectedCategories.length > 0
                onClicked: page.refreshPreview()
                objectName: "refreshReportPreviewButton"
            }
        }
        Panel {
            ui: page.ui
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: page.ui.space3
            SectionHeader {
                ui: page.ui
                title: page.ui.t("导出预览", "Export preview")
                StatusChip {
                    ui: page.ui
                    text: includeSensitive.checked ? page.ui.t("包含隐私字段", "Private fields included") : page.ui.t("隐私脱敏已开启", "Privacy redaction on")
                    tone: includeSensitive.checked ? page.ui.warning : page.ui.green
                }
            }
            Label {
                text: page.ui.t("预览与导出采用相同的类别、格式和隐私选项。", "Preview and export use the same category, format and privacy options.")
                color: page.ui.muted
                font.pixelSize: page.ui.fontCaption
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(380, page.height - 290)
                clip: true
                TextArea {
                    id: preview
                    objectName: "reportPreview"
                    text: page.previewText || (page.selectedCategories.length
                          ? page.ui.t("此格式暂时没有可显示的预览。", "No preview is available for this format yet.")
                          : page.ui.t("选择至少一个硬件类别以生成报告预览。", "Choose at least one hardware category to preview a report."))
                    textFormat: page.format === "html" ? TextEdit.RichText : TextEdit.PlainText
                    readOnly: true
                    selectByMouse: true
                    color: page.format === "html" ? page.ui.documentText : page.ui.text
                    font.family: page.format === "html" ? page.ui.fontFamily : page.ui.monoFamily
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: TextEdit.WrapAnywhere
                    background: Rectangle {
                        color: page.format === "html" ? page.ui.documentBackground : page.ui.soft
                        radius: page.ui.radiusMd
                        border.width: page.ui.borderWidth
                        border.color: page.ui.border
                    }
                    padding: page.ui.space4
                }
            }
            NoticeBanner {
                ui: page.ui
                Layout.fillWidth: true
                compact: true
                visible: !!reports.error
                text: page.ui.trKey(reports.errorKey || "", null, reports.error)
                tone: page.ui.warning
                surfaceColor: page.ui.warningSoft
            }
            NoticeBanner {
                ui: page.ui
                Layout.fillWidth: true
                compact: true
                visible: !!reports.lastPath
                icon: "check"
                text: page.ui.t("已保存至：", "Saved to: ") + reports.lastPath
                tone: page.ui.green
                surfaceColor: page.ui.positiveSoft
                textObjectName: "reportSavedPathLabel"
            }
        }
    }
}
