pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
    PageHeader {
        ui: page.ui; title: page.ui.t("偏好设置", "Make it your own")
        description: page.ui.t("舒适的视觉与熟悉的语言，所有更改即时生效并自动保存。", "Your preferred appearance and language. Changes apply and save immediately.")
        eyebrow: page.ui.t("设置 · 个性化工作空间", "PREFERENCES · YOUR WORKSPACE")
    }
    Panel {
        ui: page.ui; Layout.fillWidth: true; spacing: 22
        Label { text: page.ui.t("外观", "Appearance"); color: page.ui.text; font.pixelSize: 16; font.weight: Font.DemiBold }
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Label { text: page.ui.t("界面主题", "Theme"); color: page.ui.text; font.pixelSize: 13 }
                Label { text: page.ui.t("跟随系统或选择适合当前环境的明暗模式。", "Follow your system or choose a light or dark workspace."); color: page.ui.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
            ComboBox {
                model: [page.ui.t("跟随系统", "System"), page.ui.t("浅色", "Light"), page.ui.t("深色", "Dark")]
                currentIndex: Math.max(0, ["system", "light", "dark"].indexOf(preferences.theme))
                onActivated: preferences.theme = ["system", "light", "dark"][currentIndex]
                Layout.preferredWidth: 210; objectName: "themeSelector"
            }
        }
        Rectangle { height: 1; color: page.ui.border; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true; spacing: 18
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Label { text: page.ui.t("强调色", "Accent color"); color: page.ui.text; font.pixelSize: 13 }
                Label { text: page.ui.t("用于焦点、操作与图表。", "Used for focus, actions and charts."); color: page.ui.muted; font.pixelSize: 11 }
            }
            Repeater {
                model: ["#8f7ce1", "#528ab7", "#488e7b", "#ba738b", "#a7794e"]
                delegate: Button {
                    id: swatch
                    required property string modelData
                    implicitWidth: 34; implicitHeight: 34
                    Accessible.name: page.ui.t("强调色 ", "Accent color ") + modelData
                    onClicked: preferences.accent = modelData
                    contentItem: Icon { name: "check"; color: "white"; visible: preferences.accent.toLowerCase() === swatch.modelData; width: 16; height: 16 }
                    background: Rectangle { radius: 17; color: swatch.modelData; border.width: swatch.activeFocus ? 3 : 0; border.color: page.ui.text }
                    objectName: "accent_" + modelData.slice(1)
                }
            }
        }
        Rectangle { height: 1; color: page.ui.border; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Label { text: page.ui.t("减少动态效果", "Reduce motion"); color: page.ui.text; font.pixelSize: 13 }
                Label { text: page.ui.t("立即切换颜色与状态，避免过渡动画。", "Switch colors and states immediately, without animated transitions."); color: page.ui.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
            Switch { checked: preferences.reducedMotion; onToggled: preferences.reducedMotion = checked; Accessible.name: page.ui.t("减少动态效果", "Reduce motion"); objectName: "reducedMotionSwitch" }
        }
    }
    Panel {
        ui: page.ui; Layout.fillWidth: true; spacing: 22
        Label { text: page.ui.t("语言与地区", "Language"); color: page.ui.text; font.pixelSize: 16; font.weight: Font.DemiBold }
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Label { text: page.ui.t("界面语言", "Interface language"); color: page.ui.text; font.pixelSize: 13 }
                Label { text: page.ui.t("原始设备名称与采集诊断保留系统提供的文本。", "Raw device names and collector diagnostics retain their source text."); color: page.ui.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
            ComboBox { model: ["简体中文", "English"]; currentIndex: preferences.language === "en" ? 1 : 0; onActivated: preferences.language = currentIndex === 0 ? "zh_CN" : "en"; Layout.preferredWidth: 210; objectName: "languageSelector" }
        }
    }
    Panel {
        ui: page.ui; Layout.fillWidth: true; spacing: 14
        Label { text: page.ui.t("运行信息", "Runtime details"); color: page.ui.text; font.pixelSize: 16; font.weight: Font.DemiBold }
        Label { text: page.ui.t("渲染后端：", "Rendering backend: ") + (hardware.renderBackend || "—"); color: page.ui.secondary; font.pixelSize: 12; objectName: "settingsRenderBackendLabel" }
        Label { text: page.ui.t("数据来源：Linux 内核接口与当前用户可读取的设备信息。", "Data sources: Linux kernel interfaces and hardware information readable by the current user."); color: page.ui.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: page.ui.t("缺少的指标保留不可用状态；本应用不生成模拟设备或历史数据。", "Missing metrics retain their unavailable state. Device and history data come from actual collection."); color: page.ui.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Repeater {
            model: testRunner.capabilities
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Label { text: modelData.name; color: page.ui.text; font.pixelSize: 11; Layout.preferredWidth: 110 }
                Label { text: modelData.available ? page.ui.t("可用", "Available") : page.ui.t("未安装", "Not installed"); color: modelData.available ? page.ui.green : page.ui.warning; font.pixelSize: 11; Layout.preferredWidth: 105 }
                Label { text: modelData.path || "—"; color: page.ui.muted; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
            }
        }
    }
}
