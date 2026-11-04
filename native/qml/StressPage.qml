pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
    readonly property string mode: ["cpu", "memory", "combined"][modeBox.currentIndex]
    readonly property bool available: ui.capability("stress-ng")
    PageHeader {
        ui: page.ui; title: page.ui.t("压力测试", "Put your system to work")
        description: page.ui.t("有边界的负载、实时观察，以及随时可用的停止操作。", "Bounded workloads, live observation and a stop button within reach.")
        eyebrow: "STRESS-NG / " + page.ui.t("受控负载", "CONTROLLED WORKLOAD")
    }
    Label { visible: !page.available; text: page.ui.t("未找到可选工具 stress-ng。安装后重新启动 NekoScope 即可使用压力测试。", "Optional tool stress-ng was not found. Install it and restart NekoScope to enable stress tests."); color: page.ui.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
    RowLayout {
        Layout.fillWidth: true; spacing: 18; Layout.alignment: Qt.AlignTop
        Panel {
            ui: page.ui; Layout.preferredWidth: page.width > 1100 ? 335 : 285; Layout.alignment: Qt.AlignTop; spacing: 17
            Label { text: page.ui.t("测试配置", "Workload configuration"); color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
            Label { text: page.ui.t("负载类型", "Workload"); color: page.ui.secondary; font.pixelSize: 11 }
            ComboBox { id: modeBox; model: [page.ui.t("处理器", "CPU"), page.ui.t("内存", "Memory"), page.ui.t("处理器 + 内存", "CPU + memory")]; enabled: !testRunner.state.running; Layout.fillWidth: true; objectName: "stressModeSelector" }
            RowLayout {
                Layout.fillWidth: true
                Label { text: page.ui.t("CPU 负载", "CPU load"); color: page.ui.secondary; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Label { text: Math.round(loadSlider.value) + "%"; color: page.ui.accent; font.pixelSize: 13; font.weight: Font.DemiBold }
            }
            Slider { id: loadSlider; from: 10; to: 100; value: 25; stepSize: 5; enabled: !testRunner.state.running && page.mode !== "memory"; Layout.fillWidth: true; objectName: "stressLoadSlider" }
            GridLayout {
                columns: 2; columnSpacing: 14; rowSpacing: 10; Layout.fillWidth: true
                Label { text: page.ui.t("工作线程", "Threads"); color: page.ui.secondary; font.pixelSize: 11 }
                Label { text: page.ui.t("持续时间 (秒)", "Duration (s)"); color: page.ui.secondary; font.pixelSize: 11 }
                SpinBox { id: threadsInput; from: 1; to: Math.max(1, hardware.system.logicalCpus || 1); value: 1; editable: true; Layout.fillWidth: true; enabled: !testRunner.state.running; objectName: "stressThreadsInput" }
                SpinBox { id: durationInput; from: 1; to: 3600; value: 30; editable: true; Layout.fillWidth: true; enabled: !testRunner.state.running; objectName: "stressDurationInput" }
            }
            Label { text: page.ui.t("内存预算 (MiB)", "Memory budget (MiB)"); color: page.ui.secondary; font.pixelSize: 11 }
            SpinBox { id: memoryInput; from: 16; to: 4096; value: 256; stepSize: 16; editable: true; enabled: !testRunner.state.running && page.mode !== "cpu"; Layout.fillWidth: true; objectName: "stressMemoryInput" }
            Rectangle { height: 1; color: page.ui.border; Layout.fillWidth: true; Layout.topMargin: 2 }
            Label { text: page.ui.t("内存用量还会由后端按系统可用内存限制。测试会增加功耗与温度。", "The runner further limits memory to available resources. Testing increases power use and temperature."); color: page.ui.muted; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { text: page.ui.t("温度保护仅在当前 CPU 温度可读取且未过期时有效；缺失时不能保证热保护。", "Thermal protection requires a current readable CPU temperature. It cannot be guaranteed when readings are unavailable."); color: page.ui.warning; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ActionButton {
                ui: page.ui; text: page.ui.t("开始压力测试", "Start stress test"); primary: true; symbol: "play"; Layout.fillWidth: true
                enabled: page.available && !testRunner.state.running
                onClicked: testRunner.startStress(page.mode, Math.round(loadSlider.value), threadsInput.value, durationInput.value, memoryInput.value)
                objectName: "startStressButton"
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.alignment: Qt.AlignTop; spacing: 18
            TestStatus { ui: page.ui; kind: "stress"; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true; spacing: 12
                MetricCard { ui: page.ui; title: page.ui.t("CPU 负载", "CPU load"); value: page.ui.metric(sensors.summary.cpuUsage, 1); unit: "%"; detail: page.ui.t("当前系统采样", "Current system sample") }
                MetricCard { ui: page.ui; title: page.ui.t("CPU 温度", "CPU temperature"); value: page.ui.metric(sensors.summary.cpuTemperature, 1); unit: "°C"; detail: page.ui.t("无读数时显示 —", "— when no reading is exposed"); tone: page.ui.warning }
            }
            Panel {
                ui: page.ui; Layout.fillWidth: true
                Label { text: page.ui.t("实时 CPU 负载", "Live CPU activity"); color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                HistoryChart { ui: page.ui; samples: sensors.cpuHistory; Layout.fillWidth: true; Layout.preferredHeight: 190 }
            }
        }
    }
}
