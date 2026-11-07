pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
    readonly property string mode: ["cpu", "memory", "combined"][modeBox.currentIndex]
    readonly property bool available: ui.capability("stress-ng")
    PageHeader {
        ui: page.ui
        title: page.ui.t("压力测试", "Put your system to work")
        description: page.ui.t("有边界的负载、实时观察，以及随时可用的停止操作。", "Bounded workloads, live observation and a stop button within reach.")
        eyebrow: "STRESS-NG / " + page.ui.t("受控负载", "CONTROLLED WORKLOAD")
    }
    NoticeBanner {
        ui: page.ui
        Layout.fillWidth: true
        visible: !page.available
        text: page.ui.t("未找到可选工具 stress-ng。安装后重新启动 NekoScope 即可使用压力测试。", "Optional tool stress-ng was not found. Install it and restart NekoScope to enable stress tests.")
        tone: page.ui.warning
        surfaceColor: page.ui.warningSoft
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: page.ui.space5
        Layout.alignment: Qt.AlignTop
        Panel {
            ui: page.ui
            Layout.preferredWidth: page.width > 1100 ? 340 : 292
            Layout.alignment: Qt.AlignTop
            spacing: page.ui.space3
            SectionHeader {
                ui: page.ui
                title: page.ui.t("测试配置", "Workload configuration")
            }
            Label {
                text: page.ui.t("负载类型", "Workload")
                color: page.ui.secondary
                font.pixelSize: page.ui.fontSmall
                font.weight: Font.Medium
            }
            SelectBox {
                id: modeBox
                ui: page.ui
                model: [page.ui.t("处理器", "CPU"), page.ui.t("内存", "Memory"), page.ui.t("处理器 + 内存", "CPU + memory")]
                enabled: !testRunner.state.running
                Layout.fillWidth: true
                objectName: "stressModeSelector"
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: page.ui.space1
                Label {
                    text: page.ui.t("CPU 负载", "CPU load")
                    color: page.ui.secondary
                    font.pixelSize: page.ui.fontSmall
                    font.weight: Font.Medium
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: Math.round(loadSlider.value) + "%"
                    color: page.ui.accent
                    font.pixelSize: page.ui.fontSubtitle
                    font.weight: Font.DemiBold
                }
            }
            Slider {
                id: loadSlider
                from: 10
                to: 100
                value: 25
                stepSize: 5
                enabled: !testRunner.state.running && page.mode !== "memory"
                Layout.fillWidth: true
                implicitHeight: 32
                objectName: "stressLoadSlider"
                background: Rectangle {
                    x: loadSlider.leftPadding
                    y: loadSlider.topPadding + loadSlider.availableHeight / 2 - height / 2
                    width: loadSlider.availableWidth
                    height: 4
                    radius: 2
                    color: loadSlider.enabled ? page.ui.soft : page.ui.borderSubtle
                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(1, (loadSlider.value - loadSlider.from) / (loadSlider.to - loadSlider.from)))
                        height: parent.height
                        radius: 2
                        color: loadSlider.enabled ? page.ui.accent : page.ui.borderStrong
                    }
                }
                handle: Rectangle {
                    x: loadSlider.leftPadding + loadSlider.visualPosition * (loadSlider.availableWidth - width)
                    y: loadSlider.topPadding + loadSlider.availableHeight / 2 - height / 2
                    implicitWidth: 18
                    implicitHeight: 18
                    radius: page.ui.radiusPill
                    color: page.ui.panel
                    border.width: 2
                    border.color: loadSlider.activeFocus ? page.ui.focusRing : (loadSlider.enabled ? page.ui.accent : page.ui.borderStrong)
                    Rectangle {
                        anchors.centerIn: parent
                        implicitWidth: 6
                        implicitHeight: 6
                        radius: page.ui.radiusPill
                        color: loadSlider.enabled ? page.ui.accent : page.ui.borderStrong
                    }
                }
            }
            GridLayout {
                columns: 2
                columnSpacing: page.ui.space3
                rowSpacing: page.ui.space2
                Layout.fillWidth: true
                Label {
                    text: page.ui.t("工作线程", "Threads")
                    color: page.ui.secondary
                    font.pixelSize: page.ui.fontSmall
                    font.weight: Font.Medium
                }
                Label {
                    text: page.ui.t("持续时间 (秒)", "Duration (s)")
                    color: page.ui.secondary
                    font.pixelSize: page.ui.fontSmall
                    font.weight: Font.Medium
                }
                NumberField {
                    id: threadsInput
                    ui: page.ui
                    from: 1
                    to: Math.max(1, hardware.system.logicalCpus || 1)
                    value: 1
                    editable: true
                    enabled: !testRunner.state.running
                    Layout.fillWidth: true
                    objectName: "stressThreadsInput"
                }
                NumberField {
                    id: durationInput
                    ui: page.ui
                    from: 1
                    to: 3600
                    value: 30
                    editable: true
                    enabled: !testRunner.state.running
                    Layout.fillWidth: true
                    objectName: "stressDurationInput"
                }
            }
            Label {
                text: page.ui.t("内存预算 (MiB)", "Memory budget (MiB)")
                color: page.ui.secondary
                font.pixelSize: page.ui.fontSmall
                font.weight: Font.Medium
            }
            NumberField {
                id: memoryInput
                ui: page.ui
                from: 16
                to: 4096
                value: 256
                stepSize: 16
                editable: true
                enabled: !testRunner.state.running && page.mode !== "cpu"
                Layout.fillWidth: true
                objectName: "stressMemoryInput"
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: page.ui.borderWidth
                color: page.ui.borderSubtle
                Layout.topMargin: page.ui.space1
            }
            Label {
                text: page.ui.t("内存用量还会由后端按系统可用内存限制。测试会增加功耗与温度。", "The runner further limits memory to available resources. Testing increases power use and temperature.")
                color: page.ui.muted
                font.pixelSize: page.ui.fontCaption
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            NoticeBanner {
                ui: page.ui
                Layout.fillWidth: true
                compact: true
                text: page.ui.t("温度保护仅在当前 CPU 温度可读取且未过期时有效；缺失时不能保证热保护。", "Thermal protection requires a current readable CPU temperature. It cannot be guaranteed when readings are unavailable.")
                tone: page.ui.warning
                surfaceColor: page.ui.warningSoft
            }
            ActionButton {
                ui: page.ui
                text: page.ui.t("开始压力测试", "Start stress test")
                primary: true
                symbol: "play"
                Layout.fillWidth: true
                Layout.topMargin: page.ui.space1
                enabled: page.available && !testRunner.state.running
                onClicked: testRunner.startStress(page.mode, Math.round(loadSlider.value), threadsInput.value, durationInput.value, memoryInput.value)
                objectName: "startStressButton"
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: page.ui.space5
            TestStatus { ui: page.ui; kind: "stress"; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                spacing: page.ui.space4
                MetricCard {
                    ui: page.ui
                    title: page.ui.t("CPU 负载", "CPU load")
                    value: page.ui.metric(sensors.summary.cpuUsage, 1)
                    unit: "%"
                    detail: page.ui.t("当前系统采样", "Current system sample")
                }
                MetricCard {
                    ui: page.ui
                    title: page.ui.t("CPU 温度", "CPU temperature")
                    value: page.ui.metric(sensors.summary.cpuTemperature, 1)
                    unit: "°C"
                    detail: page.ui.t("无读数时显示 —", "— when no reading is exposed")
                    tone: page.ui.warning
                }
            }
            Panel {
                ui: page.ui
                Layout.fillWidth: true
                SectionHeader {
                    ui: page.ui
                    title: page.ui.t("实时 CPU 负载", "Live CPU activity")
                    detail: page.ui.t("当前会话采样", "This session")
                }
                HistoryChart {
                    ui: page.ui
                    samples: sensors.cpuHistory
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                }
            }
        }
    }
}
