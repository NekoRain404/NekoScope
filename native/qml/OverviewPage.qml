pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
    required property var appWindow
    readonly property var cpu: hardware.devices.find(device => device.category === "CPU") || ({})
    PageHeader {
        ui: page.ui
        title: page.ui.t("系统概览", "Your system, at a glance")
        description: page.ui.t("认识你的设备，关注每一刻的真实状态。", "A clear view of your hardware and its live activity.")
        eyebrow: page.ui.t("你的设备 · 实时洞察", "YOUR DEVICE · LIVE INSIGHTS")
        ActionButton { ui: page.ui; text: page.ui.t("生成报告", "Create report"); symbol: "report"; primary: true; onClicked: page.appWindow.navigateTo(5) }
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 178
        radius: 14
        color: "#292637"
        clip: true
        Rectangle { width: 240; height: 240; radius: 120; x: parent.width - 200; y: -120; color: page.ui.accent; opacity: 0.08 }
        RowLayout {
            anchors.fill: parent; anchors.margins: 26; spacing: 24
            ColumnLayout {
                Layout.fillWidth: true; spacing: 12
                Label { text: page.ui.t("当前系统", "CURRENT SYSTEM"); color: "#aca5c2"; font.pixelSize: 10; font.letterSpacing: 1.4 }
                Label { text: hardware.system.os || page.ui.t("正在识别系统…", "Identifying system…"); color: "#f6f2ff"; font.pixelSize: 23; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                Label { text: page.cpu.name || page.ui.t("处理器信息暂不可用", "Processor information is unavailable"); color: "#b8b2cb"; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                RowLayout {
                    spacing: 15
                    Label { text: (hardware.system.architecture || "—") + " · " + (hardware.system.kernel || "—"); color: "#9991af"; font.pixelSize: 10 }
                    Label { text: hardware.devices.length + page.ui.t(" 个已发现设备", " devices discovered"); color: "#b6a4e5"; font.pixelSize: 10 }
                }
            }
            Rectangle {
                implicitWidth: 112; implicitHeight: 112; radius: 56; color: "transparent"; border.color: "#4c435f"; border.width: 6
                Column {
                    anchors.centerIn: parent; spacing: 3
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: page.ui.metric(sensors.summary.cpuUsage, 0) + "%"; color: "#e1d5ff"; font.pixelSize: 25; font.weight: Font.DemiBold }
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: page.ui.t("CPU 使用率", "CPU usage"); color: "#a9a0bb"; font.pixelSize: 10 }
                }
            }
        }
    }
    Label { visible: !!hardware.error; text: hardware.error; color: page.ui.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
    RowLayout {
        Layout.fillWidth: true
        Label { text: page.ui.t("关键指标", "Live essentials"); color: page.ui.text; font.pixelSize: 15; font.weight: Font.DemiBold }
        Label { text: page.ui.t("来自本机传感器", "From local sensors"); color: page.ui.muted; font.pixelSize: 10; Layout.leftMargin: 6 }
        Item { Layout.fillWidth: true }
        ActionButton { ui: page.ui; compact: true; text: page.ui.t("全部传感器", "All sensors"); symbol: "arrow"; onClicked: page.appWindow.navigateTo(2) }
    }
    GridLayout {
        columns: page.width >= 1080 ? 4 : 2
        columnSpacing: 15; rowSpacing: 15; Layout.fillWidth: true
        MetricCard { ui: page.ui; title: page.ui.t("处理器温度", "CPU temperature"); value: page.ui.metric(sensors.summary.cpuTemperature, 0); unit: "°C"; detail: sensors.summary.cpuTemperature >= 0 ? page.ui.t("可读取的 CPU 温度通道", "Readable CPU temperature channel") : page.ui.t("系统未暴露 CPU 温度", "CPU temperature is not exposed"); tone: page.ui.warning }
        MetricCard { ui: page.ui; title: page.ui.t("内存使用", "Memory used"); value: page.ui.metric(sensors.summary.memoryTotalGiB > 0 ? sensors.summary.memoryUsedGiB : -1, 1); unit: "GiB"; detail: page.ui.t("总计 ", "Total ") + page.ui.metric(sensors.summary.memoryTotalGiB > 0 ? sensors.summary.memoryTotalGiB : -1, 1) + " GiB"; tone: page.ui.green }
        MetricCard { ui: page.ui; title: page.ui.t("处理器频率", "CPU frequency"); value: page.ui.metric(sensors.summary.cpuFrequencyGHz, 2); unit: "GHz"; detail: page.ui.t("内核报告的瞬时频率", "Current frequency reported by kernel") }
        MetricCard { ui: page.ui; title: page.ui.t("可用传感器", "Available sensors"); value: String(sensors.summary.sensorCount || 0); unit: page.ui.t("项", "channels"); detail: sensors.paused ? page.ui.t("显示已暂停", "Display paused") : page.ui.t("持续采集实际系统指标", "Sampling actual system metrics"); tone: page.ui.green }
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 18; Layout.alignment: Qt.AlignTop
        Panel {
            ui: page.ui; Layout.fillWidth: true; Layout.preferredWidth: 3; Layout.alignment: Qt.AlignTop
            RowLayout {
                Layout.fillWidth: true
                Label { text: page.ui.t("处理器负载", "Processor activity"); color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                Item { Layout.fillWidth: true }
                Rectangle { implicitWidth: 6; implicitHeight: 6; radius: 3; color: page.ui.accent }
                Label { text: "CPU"; color: page.ui.secondary; font.pixelSize: 10 }
            }
            Label { text: page.ui.t("当前会话的实际采样记录", "Actual samples from this session"); color: page.ui.muted; font.pixelSize: 10 }
            HistoryChart { ui: page.ui; samples: sensors.cpuHistory; Layout.fillWidth: true; Layout.preferredHeight: 215 }
        }
        Panel {
            ui: page.ui; Layout.fillWidth: true; Layout.preferredWidth: 2; Layout.alignment: Qt.AlignTop
            Label { text: page.ui.t("硬件速览", "Hardware highlights"); color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
            Repeater {
                model: ["CPU", "内存", "显卡", "存储"]
                delegate: ColumnLayout {
                    required property string modelData
                    readonly property var device: hardware.devices.find(item => item.category === modelData) || ({})
                    Layout.fillWidth: true; spacing: 6
                    Label { text: page.ui.category(modelData); color: page.ui.muted; font.pixelSize: 10 }
                    Label { text: device.name || page.ui.t("未发现可读取设备", "No readable device found"); color: page.ui.text; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: page.ui.border; Layout.topMargin: 4 }
                }
            }
            ActionButton { ui: page.ui; text: page.ui.t("浏览完整清单", "Explore inventory"); symbol: "arrow"; compact: true; Layout.fillWidth: true; onClicked: page.appWindow.navigateTo(1) }
        }
    }
}
