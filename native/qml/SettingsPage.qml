pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
    property var pageNames: []
    PageHeader {
        ui: page.ui
        title: page.ui.t("偏好设置", "Make it your own")
        description: page.ui.t("舒适的视觉与熟悉的语言，所有更改即时生效并自动保存。", "Your preferred appearance and language. Changes apply and save immediately.")
        eyebrow: page.ui.t("设置 · 个性化工作空间", "PREFERENCES · YOUR WORKSPACE")
    }
    Panel {
        ui: page.ui
        Layout.fillWidth: true
        spacing: page.ui.space4
        SectionHeader {
            ui: page.ui
            title: page.ui.t("外观", "Appearance")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space5
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.ui.t("界面主题", "Theme")
                    color: page.ui.text
                    font.pixelSize: page.ui.fontBody
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("跟随系统或选择适合当前环境的明暗模式。", "Follow your system or choose a light or dark workspace.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            SelectBox {
                model: [page.ui.t("跟随系统", "System"), page.ui.t("浅色", "Light"), page.ui.t("深色", "Dark")]
                currentIndex: Math.max(0, ["system", "light", "dark"].indexOf(preferences.theme))
                onActivated: preferences.theme = ["system", "light", "dark"][currentIndex]
                Layout.preferredWidth: 210
                ui: page.ui
                objectName: "themeSelector"
            }
        }
        Rectangle {
            height: page.ui.borderWidth
            color: page.ui.borderSubtle
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space4
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.ui.t("强调色", "Accent color")
                    color: page.ui.text
                    font.pixelSize: page.ui.fontBody
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("用于焦点、操作与图表。", "Used for focus, actions and charts.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            Row {
                spacing: page.ui.space2
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                Repeater {
                    model: ["#8f7ce1", "#528ab7", "#488e7b", "#ba738b", "#a7794e"]
                    delegate: Button {
                        id: swatch
                        required property string modelData
                        implicitWidth: 36
                        implicitHeight: 36
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: page.ui.t("强调色 ", "Accent color ") + modelData
                        onClicked: preferences.accent = modelData
                        contentItem: Icon {
                            name: "check"
                            color: page.ui.accentText
                            visible: preferences.accent.toLowerCase() === swatch.modelData
                            implicitWidth: 16
                            implicitHeight: 16
                        }
                        background: Rectangle {
                            radius: page.ui.radiusPill
                            color: swatch.modelData
                            border.width: preferences.accent.toLowerCase() === swatch.modelData || swatch.activeFocus ? 2 : 0
                            border.color: swatch.activeFocus ? page.ui.text : page.ui.accentText
                        }
                        objectName: "accent_" + modelData.slice(1)
                    }
                }
            }
        }
        Rectangle {
            height: page.ui.borderWidth
            color: page.ui.borderSubtle
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space5
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.ui.t("减少动态效果", "Reduce motion")
                    color: page.ui.text
                    font.pixelSize: page.ui.fontBody
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("立即切换颜色与状态，避免过渡动画。", "Switch colors and states immediately, without animated transitions.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            ToggleSwitch {
                ui: page.ui
                checked: preferences.reducedMotion
                onToggled: preferences.reducedMotion = checked
                Accessible.name: page.ui.t("减少动态效果", "Reduce motion")
                objectName: "reducedMotionSwitch"
            }
        }
    }
    Panel {
        ui: page.ui
        Layout.fillWidth: true
        spacing: page.ui.space4
        SectionHeader {
            ui: page.ui
            title: page.ui.t("传感器与数据", "Sensors & data")
            detail: page.ui.t("更改即时生效并自动保存", "Changes apply and save immediately")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space5
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.ui.t("采样间隔", "Sample interval")
                    color: page.ui.text
                    font.pixelSize: page.ui.fontBody
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("更短的间隔更实时，但会增加唤醒与系统读取。", "Shorter intervals are more responsive but wake the system more often.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            SelectBox {
                ui: page.ui
                model: ["0.5 s", "1 s", "2 s", "5 s"]
                currentIndex: Math.max(0, [500, 1000, 2000, 5000].indexOf(preferences.sensorIntervalMs))
                onActivated: preferences.sensorIntervalMs = [500, 1000, 2000, 5000][currentIndex]
                Layout.preferredWidth: 210
                objectName: "sensorIntervalSelector"
            }
        }
        Rectangle { height: page.ui.borderWidth; color: page.ui.borderSubtle; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space5
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.ui.t("历史采样数", "History samples")
                    color: page.ui.text
                    font.pixelSize: page.ui.fontBody
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("图表与统计保留的最近采样数量。", "Number of recent samples kept for charts and statistics.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            SelectBox {
                ui: page.ui
                model: [30, 60, 90, 150, 300]
                currentIndex: Math.max(0, [30, 60, 90, 150, 300].indexOf(preferences.historySamples))
                onActivated: preferences.historySamples = [30, 60, 90, 150, 300][currentIndex]
                Layout.preferredWidth: 210
                objectName: "historySamplesSelector"
            }
        }
        Rectangle { height: page.ui.borderWidth; color: page.ui.borderSubtle; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space5
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.ui.t("温度单位", "Temperature unit")
                    color: page.ui.text
                    font.pixelSize: page.ui.fontBody
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("应用于传感器、图表与硬件温度字段。", "Applied to sensors, charts and hardware temperature fields.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            SelectBox {
                ui: page.ui
                model: [page.ui.t("摄氏度 (°C)", "Celsius (°C)"), page.ui.t("华氏度 (°F)", "Fahrenheit (°F)")]
                currentIndex: preferences.temperatureUnit === "fahrenheit" ? 1 : 0
                onActivated: preferences.temperatureUnit = currentIndex === 0 ? "celsius" : "fahrenheit"
                Layout.preferredWidth: 210
                objectName: "temperatureUnitSelector"
            }
        }
        Rectangle { height: page.ui.borderWidth; color: page.ui.borderSubtle; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space5
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.ui.t("显示诊断信息", "Show diagnostics")
                    color: page.ui.text
                    font.pixelSize: page.ui.fontBody
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("在字段下显示来源、状态与读取原因。", "Show source, status and read reasons under each field.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            ToggleSwitch {
                ui: page.ui
                checked: preferences.showDiagnostics
                onToggled: preferences.showDiagnostics = checked
                Accessible.name: page.ui.t("显示诊断信息", "Show diagnostics")
                objectName: "showDiagnosticsSwitch"
            }
        }
        Rectangle { height: page.ui.borderWidth; color: page.ui.borderSubtle; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space5
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.ui.t("启动页面", "Start page")
                    color: page.ui.text
                    font.pixelSize: page.ui.fontBody
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("每次启动应用时首先打开的页面。", "The page opened when the application starts.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            SelectBox {
                ui: page.ui
                model: page.pageNames
                currentIndex: preferences.startPage
                onActivated: preferences.startPage = currentIndex
                Layout.preferredWidth: 210
                enabled: page.pageNames.length > 0
                objectName: "startPageSelector"
            }
        }
        Rectangle { height: page.ui.borderWidth; color: page.ui.borderSubtle; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space5
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.ui.t("恢复默认设置", "Reset preferences")
                    color: page.ui.text
                    font.pixelSize: page.ui.fontBody
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("重置外观、语言、传感器与显示选项。", "Reset appearance, language, sensor and display options.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            ActionButton {
                ui: page.ui
                danger: true
                text: page.ui.t("恢复默认", "Reset")
                onClicked: resetDialog.open()
                objectName: "resetPreferencesButton"
            }
        }
    }
    Panel {
        ui: page.ui
        Layout.fillWidth: true
        spacing: page.ui.space4
        SectionHeader {
            ui: page.ui
            title: page.ui.t("语言与地区", "Language")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space5
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.ui.t("界面语言", "Interface language")
                    color: page.ui.text
                    font.pixelSize: page.ui.fontBody
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("原始设备名称与采集诊断保留系统提供的文本。", "Raw device names and collector diagnostics retain their source text.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            SelectBox {
                model: ["简体中文", "English"]
                currentIndex: preferences.language === "en" ? 1 : 0
                onActivated: preferences.language = currentIndex === 0 ? "zh_CN" : "en"
                Layout.preferredWidth: 210
                ui: page.ui
                objectName: "languageSelector"
            }
        }
    }
    Panel {
        ui: page.ui
        Layout.fillWidth: true
        spacing: page.ui.space3
        SectionHeader {
            ui: page.ui
            title: page.ui.t("运行信息", "Runtime details")
        }
        Label {
            text: page.ui.t("渲染后端：", "Rendering backend: ") + (hardware.renderBackend || "—")
            color: page.ui.secondary
            font.pixelSize: page.ui.fontSmall
            objectName: "settingsRenderBackendLabel"
        }
        Label {
            text: page.ui.t("数据来源：Linux 内核接口与当前用户可读取的设备信息。", "Data sources: Linux kernel interfaces and hardware information readable by the current user.")
            color: page.ui.muted
            font.pixelSize: page.ui.fontCaption
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            text: page.ui.t("缺少的指标保留不可用状态；本应用不生成模拟设备或历史数据。", "Missing metrics retain their unavailable state. Device and history data come from actual collection.")
            color: page.ui.muted
            font.pixelSize: page.ui.fontCaption
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: page.ui.borderWidth
            color: page.ui.borderSubtle
            Layout.topMargin: page.ui.space1
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: page.ui.space2
            Layout.rightMargin: page.ui.space2
            spacing: page.ui.space3
            Label {
                text: page.ui.t("可选工具", "OPTIONAL TOOL")
                color: page.ui.muted
                font.pixelSize: page.ui.fontMicro
                font.weight: Font.Medium
                font.letterSpacing: 0.8
                Layout.preferredWidth: 130
            }
            Label {
                text: page.ui.t("状态", "STATUS")
                color: page.ui.muted
                font.pixelSize: page.ui.fontMicro
                font.weight: Font.Medium
                font.letterSpacing: 0.8
                Layout.preferredWidth: 120
            }
            Label {
                text: page.ui.t("路径", "PATH")
                color: page.ui.muted
                font.pixelSize: page.ui.fontMicro
                font.weight: Font.Medium
                font.letterSpacing: 0.8
                Layout.fillWidth: true
            }
        }
        Repeater {
            model: testRunner.capabilities
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Layout.leftMargin: page.ui.space2
                Layout.rightMargin: page.ui.space2
                spacing: page.ui.space3
                Label {
                    text: modelData.name
                    color: page.ui.text
                    font.pixelSize: page.ui.fontSmall
                    font.weight: Font.Medium
                    Layout.preferredWidth: 130
                }
                StatusChip {
                    ui: page.ui
                    text: modelData.available ? page.ui.t("可用", "Available") : page.ui.t("未安装", "Not installed")
                    tone: modelData.available ? page.ui.green : page.ui.warning
                    Layout.preferredWidth: 120
                }
                Label {
                    text: modelData.path || "—"
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontCaption
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }
        EmptyState {
            Layout.fillWidth: true
            Layout.topMargin: page.ui.space2
            Layout.bottomMargin: page.ui.space2
            visible: !testRunner.capabilities.length
            ui: page.ui
            icon: "info"
            title: page.ui.t("没有可选工具信息", "No optional tools reported")
            message: page.ui.t("压力测试与基准测试需要外部工具；硬件与传感器数据不依赖它们。", "Stress and benchmark tests need external tools; hardware and sensor data do not.")
        }
    }
    Popup {
        id: resetDialog
        modal: true
        focus: true
        anchors.centerIn: parent
        width: 360
        padding: page.ui.space5
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: page.ui.panel
            radius: page.ui.radiusLg
            border.width: page.ui.borderWidth
            border.color: page.ui.borderStrong
        }
        contentItem: ColumnLayout {
            spacing: page.ui.space4
            Label {
                text: page.ui.t("恢复默认设置？", "Reset preferences?")
                color: page.ui.text
                font.pixelSize: page.ui.fontSubtitle
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            Label {
                text: page.ui.t("主题、语言、强调色、传感器与显示选项将恢复默认值。", "Theme, language, accent, sensor and display options return to their defaults.")
                color: page.ui.secondary
                font.pixelSize: page.ui.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: page.ui.space2
                Item { Layout.fillWidth: true }
                ActionButton {
                    ui: page.ui
                    compact: true
                    text: page.ui.t("取消", "Cancel")
                    onClicked: resetDialog.close()
                }
                ActionButton {
                    ui: page.ui
                    compact: true
                    danger: true
                    text: page.ui.t("恢复默认", "Reset")
                    onClicked: { preferences.reset(); resetDialog.close() }
                    objectName: "confirmResetPreferencesButton"
                }
            }
        }
    }

}
