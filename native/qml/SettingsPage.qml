pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
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
}
