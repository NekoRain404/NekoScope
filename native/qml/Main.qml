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
    font.family: theme.fontFamily
    font.pixelSize: theme.fontBody
    property int currentPage: 0
    property alias ui: theme
    readonly property var pageNames: [theme.t("系统概览", "Overview"), theme.t("硬件信息", "Hardware"), theme.t("实时传感器", "Sensors"), theme.t("压力测试", "Stress test"), theme.t("基准测试", "Benchmarks"), theme.t("系统报告", "Reports"), theme.t("偏好设置", "Preferences")]
    function navigateTo(index) { if (index >= 0 && index < 7) currentPage = index }
    Component.onCompleted: currentPage = preferences.startPage
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
        highlightedText: theme.accentText
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
        gradient: Gradient {
            GradientStop { position: 0; color: theme.sidebarTop }
            GradientStop { position: 1; color: theme.sidebarBottom }
        }
        Rectangle {
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: theme.borderWidth
            color: theme.sidebarBorder
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: theme.space4
            anchors.rightMargin: theme.space4
            anchors.topMargin: theme.space5
            anchors.bottomMargin: theme.space4
            spacing: theme.space1
            RowLayout {
                Layout.topMargin: theme.space2
                Layout.bottomMargin: theme.space6
                Layout.leftMargin: theme.space2
                spacing: theme.space3
                Rectangle {
                    implicitWidth: 34
                    implicitHeight: 34
                    radius: theme.radiusMd + 2
                    color: theme.accent
                    Icon {
                        anchors.centerIn: parent
                        name: "sensors"
                        color: theme.accentText
                        implicitWidth: 22
                        implicitHeight: 22
                    }
                }
                Column {
                    spacing: 1
                    Label {
                        text: "NekoScope"
                        color: theme.sidebarTextStrong
                        font.pixelSize: theme.fontHeading
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "HARDWARE INSIGHTS"
                        color: theme.sidebarMuted
                        font.pixelSize: theme.fontMicro
                        font.letterSpacing: 1.6
                    }
                }
            }
            Label {
                text: theme.t("工作空间", "WORKSPACE")
                color: theme.sidebarMuted
                font.pixelSize: theme.fontMicro
                font.weight: Font.Medium
                font.letterSpacing: 1.5
                Layout.leftMargin: theme.space3
                Layout.bottomMargin: theme.space3
            }
            Repeater {
                model: 6
                delegate: NavItem {
                    id: nav
                    required property int index
                    ui: theme
                    objectName: "nav" + index
                    Layout.fillWidth: true
                    iconName: ["overview", "hardware", "sensors", "stress", "benchmark", "report"][nav.index]
                    text: app.pageNames[nav.index]
                    selected: app.currentPage === nav.index
                    onClicked: app.navigateTo(nav.index)
                }
            }
            Item { Layout.fillHeight: true }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: jobColumn.implicitHeight + theme.space6
                radius: theme.radiusMd
                visible: testRunner.state.running
                color: theme.sidebarHover
                border.width: theme.borderWidth
                border.color: theme.sidebarBorder
                ColumnLayout {
                    id: jobColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: theme.space3 }
                    spacing: theme.space1
                    Label {
                        text: theme.t("测试进行中", "Test in progress")
                        color: theme.sidebarTextStrong
                        font.pixelSize: theme.fontSmall
                        font.weight: Font.Medium
                    }
                    Label {
                        text: (testRunner.state.progress || 0) + "% · " + testRunner.state.elapsed + " / " + testRunner.state.duration + " s"
                        color: theme.sidebarMuted
                        font.pixelSize: theme.fontMicro
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: theme.space1
                        implicitHeight: 4
                        radius: theme.radiusPill
                        color: theme.sidebarBorder
                        Rectangle {
                            width: parent.width * Math.max(0, Math.min(1, (testRunner.state.progress || 0) / 100))
                            height: parent.height
                            radius: theme.radiusPill
                            color: theme.accent
                        }
                    }
                    ActionButton {
                        ui: theme
                        text: theme.t("停止测试", "Stop test")
                        symbol: "stop"
                        primary: true
                        compact: true
                        Layout.fillWidth: true
                        Layout.topMargin: theme.space1
                        onClicked: testRunner.stop()
                        objectName: "globalStopButton"
                    }
                }
            }
            NavItem {
                id: settingsNav
                ui: theme
                objectName: "settingsButton"
                text: app.pageNames[6]
                iconName: "settings"
                selected: app.currentPage === 6
                Layout.fillWidth: true
                Layout.topMargin: theme.space2
                onClicked: app.navigateTo(6)
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: theme.borderWidth
                color: theme.sidebarBorder
                Layout.topMargin: theme.space3
                Layout.bottomMargin: theme.space3
            }
            RowLayout {
                Layout.leftMargin: theme.space3
                spacing: theme.space2
                Rectangle {
                    implicitWidth: 6
                    implicitHeight: 6
                    radius: theme.radiusPill
                    color: hardware.error ? theme.warning : theme.green
                }
                Label {
                    text: hardware.busy ? theme.t("正在采集…", "Collecting…") : theme.t("本机实时采集", "Live local data")
                    color: theme.sidebarText
                    font.pixelSize: theme.fontMicro
                }
            }
            Label {
                text: "NekoScope · Linux"
                color: theme.sidebarMuted
                font.pixelSize: theme.fontMicro
                Layout.leftMargin: theme.space3
                Layout.topMargin: theme.space1
                Layout.bottomMargin: theme.space1
            }
        }
    }

    ColumnLayout {
        anchors { left: sidebar.right; right: parent.right; top: parent.top; bottom: parent.bottom }
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 64
            color: theme.panel
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: app.width > 1200 ? theme.space8 : theme.space6
                anchors.rightMargin: app.width > 1200 ? theme.space8 : theme.space6
                spacing: theme.space3
                Label {
                    text: theme.t("工作空间", "Workspace")
                    color: theme.muted
                    font.pixelSize: theme.fontSmall
                }
                Label {
                    text: "/"
                    color: theme.borderStrong
                    font.pixelSize: theme.fontBodyLarge
                }
                Label {
                    text: app.pageNames[app.currentPage]
                    color: theme.secondary
                    font.pixelSize: theme.fontSmall
                    font.weight: Font.Medium
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: "LINUX NATIVE"
                    color: theme.accent
                    font.pixelSize: theme.fontMicro
                    font.weight: Font.Medium
                    font.letterSpacing: 1.3
                    visible: app.width > 1100
                }
                ActionButton {
                    ui: theme
                    compact: true
                    text: theme.chinese ? "EN" : "中文"
                    onClicked: preferences.language = theme.chinese ? "en" : "zh_CN"
                    objectName: "languageToggle"
                }
                ActionButton {
                    ui: theme
                    compact: true
                    symbol: theme.dark ? "sun" : "moon"
                    text: theme.dark ? theme.t("浅色", "Light") : theme.t("深色", "Dark")
                    onClicked: preferences.theme = theme.dark ? "light" : "dark"
                    objectName: "themeToggle"
                }
                ActionButton {
                    ui: theme
                    compact: true
                    symbol: "refresh"
                    text: theme.t("刷新", "Refresh")
                    enabled: !hardware.busy
                    onClicked: hardware.refresh()
                    objectName: "refreshButton"
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: theme.borderWidth
                color: theme.border
            }
        }
        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: app.currentPage
            OverviewPage { ui: theme; appWindow: app; objectName: "overviewPage" }
            HardwarePage { ui: theme; objectName: "hardwarePage" }
            SensorsPage { ui: theme; objectName: "sensorsPage" }
            StressPage { ui: theme; objectName: "stressPage" }
            BenchmarksPage { ui: theme; objectName: "benchmarksPage" }
            ReportsPage { ui: theme; objectName: "reportsPage" }
            SettingsPage { ui: theme; pageNames: app.pageNames; objectName: "settingsPage" }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 32
            color: theme.background
            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: theme.borderWidth
                color: theme.border
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: app.width > 1200 ? theme.space8 : theme.space6
                anchors.rightMargin: app.width > 1200 ? theme.space8 : theme.space6
                Label {
                    text: theme.t("只读硬件采集 · 普通用户会话", "Read-only inventory · User session")
                    font.pixelSize: theme.fontMicro
                    color: theme.muted
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: hardware.renderBackend || theme.t("等待渲染器", "Waiting for renderer")
                    font.pixelSize: theme.fontMicro
                    color: theme.muted
                    objectName: "renderBackendLabel"
                }
            }
        }
    }
}
