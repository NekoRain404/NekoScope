pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: app
    objectName: "mainWindow"
    width: 1380
    height: 900
    minimumWidth: 1000
    minimumHeight: 700
    visible: true
    title: "NekoScope"
    color: theme.background
    font.family: "Noto Sans"
    font.pixelSize: 13
    property int currentPage: 0
    property alias ui: theme
    readonly property var pageNames: [theme.t("系统概览", "Overview"), theme.t("硬件信息", "Hardware"), theme.t("实时传感器", "Sensors"), theme.t("压力测试", "Stress test"), theme.t("基准测试", "Benchmarks"), theme.t("系统报告", "Reports"), theme.t("偏好设置", "Preferences")]
    function navigateTo(index) { if (index >= 0 && index < 7) currentPage = index }
    Theme { id: theme }
    palette {
        window: theme.background
        windowText: theme.text
        base: theme.panel
        alternateBase: theme.soft
        text: theme.text
        button: theme.soft
        buttonText: theme.text
        highlight: theme.accent
        highlightedText: "white"
        placeholderText: theme.muted
        mid: theme.border
        dark: theme.border
        light: theme.panel
    }
    Shortcut { sequence: "Ctrl+1"; onActivated: app.navigateTo(0) }
    Shortcut { sequence: "Ctrl+2"; onActivated: app.navigateTo(1) }
    Shortcut { sequence: "Ctrl+3"; onActivated: app.navigateTo(2) }
    Shortcut { sequence: "Ctrl+4"; onActivated: app.navigateTo(3) }
    Shortcut { sequence: "Ctrl+5"; onActivated: app.navigateTo(4) }
    Shortcut { sequence: "Ctrl+6"; onActivated: app.navigateTo(5) }
    Shortcut { sequence: "Ctrl+,"; onActivated: app.navigateTo(6) }
    Shortcut { sequence: "F5"; onActivated: hardware.refresh() }
    Rectangle {
        id: sidebar
        width: app.width > 1200 ? 224 : 198
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        color: "#222232"
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 8
            RowLayout {
                Layout.topMargin: 14; Layout.bottomMargin: 33; Layout.leftMargin: 7
                spacing: 11
                Rectangle {
                    implicitWidth: 32; implicitHeight: 32; radius: 10; color: theme.accent
                    Icon { anchors.centerIn: parent; name: "sensors"; color: "white"; width: 22; height: 22 }
                }
                Column {
                    spacing: 2
                    Label { text: "NekoScope"; color: "#f3f1ff"; font.pixelSize: 21; font.weight: Font.DemiBold }
                    Label { text: "HARDWARE INSIGHTS"; color: "#9290a8"; font.pixelSize: 8; font.letterSpacing: 1.5 }
                }
            }
            Label { text: theme.t("工作空间", "WORKSPACE"); color: "#77768f"; font.pixelSize: 9; font.letterSpacing: 1.5; Layout.leftMargin: 13; Layout.bottomMargin: 9 }
            Repeater {
                model: 6
                delegate: Button {
                    id: nav
                    required property int index
                    objectName: "nav" + index
                    Layout.fillWidth: true
                    implicitHeight: 44
                    text: app.pageNames[index]
                    hoverEnabled: true
                    Accessible.name: text
                    onClicked: app.navigateTo(index)
                    contentItem: RowLayout {
                        spacing: 12
                        Icon { name: ["overview", "hardware", "sensors", "stress", "benchmark", "report"][nav.index]; color: app.currentPage === nav.index ? "#d3c7ff" : "#a3a0b7"; implicitWidth: 18; implicitHeight: 18 }
                        Label { text: nav.text; color: app.currentPage === nav.index ? "#f3edff" : "#b1aec4"; font.pixelSize: 12; font.weight: app.currentPage === nav.index ? Font.DemiBold : Font.Normal; Layout.fillWidth: true }
                        Rectangle { visible: app.currentPage === nav.index; implicitWidth: 5; implicitHeight: 5; radius: 3; color: "#b5a0ff" }
                    }
                    leftPadding: 13; rightPadding: 14
                    background: Rectangle {
                        radius: 8
                        color: app.currentPage === nav.index ? "#393348" : (nav.hovered ? "#2d2b3e" : "transparent")
                        border.color: nav.activeFocus ? theme.accent : "transparent"
                        Behavior on color { ColorAnimation { duration: theme.reducedMotion ? 0 : 140 } }
                    }
                }
            }
            Item { Layout.fillHeight: true }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: jobColumn.implicitHeight + 24; radius: 9
                visible: testRunner.state.running
                color: "#302c43"; border.color: "#49405e"
                ColumnLayout {
                    id: jobColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                    Label { text: theme.t("测试进行中", "Test in progress"); color: "#d1c3f5"; font.pixelSize: 11 }
                    Label { text: testRunner.state.elapsed + " / " + testRunner.state.duration + " s"; color: "#a8a2bc"; font.pixelSize: 10 }
                    ActionButton { ui: theme; text: theme.t("停止测试", "Stop test"); symbol: "stop"; compact: true; Layout.fillWidth: true; onClicked: testRunner.stop(); objectName: "globalStopButton" }
                }
            }
            Button {
                id: settingsNav
                objectName: "settingsButton"
                text: app.pageNames[6]; Layout.fillWidth: true; implicitHeight: 44; hoverEnabled: true
                onClicked: app.navigateTo(6)
                contentItem: RowLayout {
                    spacing: 12
                    Icon { name: "settings"; color: "#a3a0b7"; implicitWidth: 18; implicitHeight: 18 }
                    Label { text: settingsNav.text; color: "#b1aec4"; font.pixelSize: 12; Layout.fillWidth: true }
                }
                leftPadding: 13
                background: Rectangle { color: settingsNav.hovered || app.currentPage === 6 ? "#302d40" : "transparent"; radius: 8; border.color: settingsNav.activeFocus ? theme.accent : "transparent" }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#373548"; Layout.topMargin: 9; Layout.bottomMargin: 12 }
            RowLayout {
                Layout.leftMargin: 10
                Rectangle { implicitWidth: 6; implicitHeight: 6; radius: 3; color: hardware.error ? "#d9a775" : "#85b6a4" }
                Label { text: hardware.busy ? theme.t("正在采集…", "Collecting…") : theme.t("本机实时采集", "Live local data"); color: "#aaa5bd"; font.pixelSize: 10 }
            }
            Label { text: "NekoScope · Linux"; color: "#737087"; font.pixelSize: 9; Layout.leftMargin: 10; Layout.topMargin: 4; Layout.bottomMargin: 7 }
        }
    }
    ColumnLayout {
        anchors { left: sidebar.right; right: parent.right; top: parent.top; bottom: parent.bottom }
        spacing: 0
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 65; color: theme.panel
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 28; anchors.rightMargin: 28; spacing: 12
                Label { text: theme.t("工作空间", "Workspace"); color: theme.muted; font.pixelSize: 11 }
                Label { text: "/"; color: theme.border; font.pixelSize: 15 }
                Label { text: app.pageNames[app.currentPage]; color: theme.secondary; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Label { text: "LINUX NATIVE"; color: theme.accent; font.pixelSize: 9; font.letterSpacing: 1.2; visible: app.width > 1100 }
                ActionButton { ui: theme; compact: true; text: theme.chinese ? "EN" : "中文"; onClicked: preferences.language = theme.chinese ? "en" : "zh_CN"; objectName: "languageToggle" }
                ActionButton { ui: theme; compact: true; symbol: "sun"; text: theme.dark ? theme.t("浅色", "Light") : theme.t("深色", "Dark"); onClicked: preferences.theme = theme.dark ? "light" : "dark"; objectName: "themeToggle" }
                ActionButton { ui: theme; compact: true; symbol: "refresh"; text: theme.t("刷新", "Refresh"); enabled: !hardware.busy; onClicked: hardware.refresh(); objectName: "refreshButton" }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.border }
        }
        StackLayout {
            id: pages
            Layout.fillWidth: true; Layout.fillHeight: true
            currentIndex: app.currentPage
            OverviewPage { ui: theme; appWindow: app; objectName: "overviewPage" }
            HardwarePage { ui: theme; objectName: "hardwarePage" }
            SensorsPage { ui: theme; objectName: "sensorsPage" }
            StressPage { ui: theme; objectName: "stressPage" }
            BenchmarksPage { ui: theme; objectName: "benchmarksPage" }
            ReportsPage { ui: theme; objectName: "reportsPage" }
            SettingsPage { ui: theme; objectName: "settingsPage" }
        }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 31; color: theme.background
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 28; anchors.rightMargin: 28
                Label { text: theme.t("只读硬件采集 · 普通用户会话", "Read-only inventory · User session"); font.pixelSize: 9; color: theme.muted }
                Item { Layout.fillWidth: true }
                Label { text: (hardware.renderBackend || theme.t("等待渲染器", "Waiting for renderer")); font.pixelSize: 9; color: theme.muted; objectName: "renderBackendLabel" }
            }
        }
    }
}
