pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

PageView {
    id: page
    required property var appWindow
    readonly property var cpu: hardware.devices.find(device => device.category === "CPU") || ({})
    readonly property real cpuUsage: Math.max(0, Math.min(100, Number(sensors.summary.cpuUsage) || 0))
    readonly property real cpuSweep: sensors.summary.cpuUsage >= 0 ? cpuUsage * 3.6 : 0
    PageHeader {
        ui: page.ui
        title: page.ui.t("系统概览", "Your system, at a glance")
        description: page.ui.t("认识你的设备，关注每一刻的真实状态。", "A clear view of your hardware and its live activity.")
        eyebrow: page.ui.t("你的设备 · 实时洞察", "YOUR DEVICE · LIVE INSIGHTS")
        ActionButton {
            ui: page.ui
            text: page.ui.t("生成报告", "Create report")
            symbol: "report"
            primary: true
            onClicked: page.appWindow.navigateTo(5)
        }
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 184
        radius: page.ui.radiusXl
        gradient: Gradient {
            GradientStop { position: 0; color: page.ui.heroTop }
            GradientStop { position: 1; color: page.ui.heroBottom }
        }
        border.width: page.ui.borderWidth
        border.color: page.ui.heroLine
        clip: true
        Rectangle {
            width: 300
            height: 300
            radius: page.ui.radiusPill
            x: parent.width - 210
            y: -140
            color: page.ui.accent
            opacity: 0.10
        }
        Rectangle {
            width: 180
            height: 180
            radius: page.ui.radiusPill
            x: parent.width - 380
            y: 100
            color: page.ui.accent
            opacity: 0.04
        }
        RowLayout {
            anchors.fill: parent
            anchors.margins: page.ui.space6 + 2
            spacing: page.ui.space6
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space2
                Label {
                    text: page.ui.t("当前系统", "CURRENT SYSTEM")
                    color: page.ui.heroMuted
                    font.pixelSize: page.ui.fontCaption
                    font.weight: Font.Medium
                    font.letterSpacing: 1.5
                }
                Label {
                    text: hardware.system.os || page.ui.t("正在识别系统…", "Identifying system…")
                    color: page.ui.heroText
                    font.pixelSize: page.ui.fontHeading
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: page.cpu.name || page.ui.t("处理器信息暂不可用", "Processor information is unavailable")
                    color: page.ui.heroSecondary
                    font.pixelSize: page.ui.fontBody
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                RowLayout {
                    Layout.topMargin: page.ui.space1
                    spacing: page.ui.space4
                    RowLayout {
                        spacing: page.ui.space1
                        Icon { name: "hardware"; color: page.ui.heroMuted; implicitWidth: 12; implicitHeight: 12 }
                        Label {
                            text: (hardware.system.architecture || "—") + " · " + (hardware.system.kernel || "—")
                            color: page.ui.heroMuted
                            font.pixelSize: page.ui.fontCaption
                        }
                    }
                    RowLayout {
                        spacing: page.ui.space1
                        Rectangle {
                            implicitWidth: 5
                            implicitHeight: 5
                            radius: page.ui.radiusPill
                            color: page.ui.accent
                        }
                        Label {
                            text: hardware.devices.length + page.ui.t(" 个已发现设备", " devices discovered")
                            color: page.ui.heroSecondary
                            font.pixelSize: page.ui.fontCaption
                        }
                    }
                }
            }
            Item {
                implicitWidth: 118
                implicitHeight: 118
                Shape {
                    anchors.fill: parent
                    antialiasing: true
                    ShapePath {
                        strokeColor: page.ui.heroLine
                        strokeWidth: 6
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathAngleArc {
                            centerX: 59
                            centerY: 59
                            radiusX: 55
                            radiusY: 55
                            startAngle: -90
                            sweepAngle: 360
                        }
                    }
                    ShapePath {
                        strokeColor: page.ui.accent
                        strokeWidth: 6
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathAngleArc {
                            centerX: 59
                            centerY: 59
                            radiusX: 55
                            radiusY: 55
                            startAngle: -90
                            sweepAngle: page.cpuSweep
                        }
                    }
                }
                Column {
                    anchors.centerIn: parent
                    spacing: 1
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: sensors.summary.cpuUsage >= 0 ? page.ui.metric(sensors.summary.cpuUsage, 0) + "%" : "—"
                        color: page.ui.heroText
                        font.pixelSize: page.ui.fontHeading
                        font.weight: Font.DemiBold
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: page.ui.t("CPU 使用率", "CPU usage")
                        color: page.ui.heroMuted
                        font.pixelSize: page.ui.fontMicro
                    }
                }
            }
        }
    }
    NoticeBanner {
        ui: page.ui
        Layout.fillWidth: true
        visible: !!hardware.error
        text: hardware.error
        tone: page.ui.warning
        surfaceColor: page.ui.warningSoft
    }
    SectionHeader {
        ui: page.ui
        title: page.ui.t("关键指标", "Live essentials")
        detail: page.ui.t("来自本机传感器", "From local sensors")
        ActionButton {
            ui: page.ui
            compact: true
            text: page.ui.t("全部传感器", "All sensors")
            symbol: "arrow"
            onClicked: page.appWindow.navigateTo(2)
        }
    }
    GridLayout {
        columns: page.width >= 1080 ? 4 : 2
        columnSpacing: page.ui.space4
        rowSpacing: page.ui.space4
        Layout.fillWidth: true
        MetricCard {
            ui: page.ui
            title: page.ui.t("处理器温度", "CPU temperature")
            value: page.ui.metric(sensors.summary.cpuTemperature, 0)
            unit: "°C"
            detail: sensors.summary.cpuTemperature >= 0 ? page.ui.t("可读取的 CPU 温度通道", "Readable CPU temperature channel") : page.ui.t("系统未暴露 CPU 温度", "CPU temperature is not exposed")
            tone: page.ui.warning
        }
        MetricCard {
            ui: page.ui
            title: page.ui.t("内存使用", "Memory used")
            value: page.ui.metric(sensors.summary.memoryTotalGiB > 0 ? sensors.summary.memoryUsedGiB : -1, 1)
            unit: "GiB"
            detail: page.ui.t("总计 ", "Total ") + page.ui.metric(sensors.summary.memoryTotalGiB > 0 ? sensors.summary.memoryTotalGiB : -1, 1) + " GiB"
            tone: page.ui.green
        }
        MetricCard {
            ui: page.ui
            title: page.ui.t("处理器频率", "CPU frequency")
            value: page.ui.metric(sensors.summary.cpuFrequencyGHz, 2)
            unit: "GHz"
            detail: page.ui.t("内核报告的瞬时频率", "Current frequency reported by kernel")
        }
        MetricCard {
            ui: page.ui
            title: page.ui.t("可用传感器", "Available sensors")
            value: String(sensors.summary.sensorCount || 0)
            unit: page.ui.t("项", "channels")
            detail: sensors.paused ? page.ui.t("显示已暂停", "Display paused") : page.ui.t("持续采集实际系统指标", "Sampling actual system metrics")
            tone: page.ui.green
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: page.ui.space5
        Layout.alignment: Qt.AlignTop
        Panel {
            ui: page.ui
            Layout.fillWidth: true
            Layout.preferredWidth: 3
            Layout.alignment: Qt.AlignTop
            SectionHeader {
                ui: page.ui
                title: page.ui.t("处理器负载", "Processor activity")
                detail: page.ui.t("当前会话的实际采样", "Actual samples from this session")
                RowLayout {
                    spacing: page.ui.space1
                    Rectangle { implicitWidth: 6; implicitHeight: 6; radius: page.ui.radiusPill; color: page.ui.accent }
                    Label { text: "CPU"; color: page.ui.secondary; font.pixelSize: page.ui.fontCaption }
                }
            }
            HistoryChart {
                ui: page.ui
                samples: sensors.cpuHistory
                Layout.fillWidth: true
                Layout.preferredHeight: 215
            }
        }
        Panel {
            ui: page.ui
            Layout.fillWidth: true
            Layout.preferredWidth: 2
            Layout.alignment: Qt.AlignTop
            SectionHeader {
                ui: page.ui
                title: page.ui.t("硬件速览", "Hardware highlights")
            }
            Repeater {
                model: ["CPU", "内存", "显卡", "存储"]
                delegate: ColumnLayout {
                    required property string modelData
                    readonly property var device: hardware.devices.find(item => item.category === modelData) || ({})
                    Layout.fillWidth: true
                    spacing: page.ui.space1
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: page.ui.category(modelData)
                            color: page.ui.muted
                            font.pixelSize: page.ui.fontCaption
                            Layout.preferredWidth: 74
                        }
                        Label {
                            text: device.name || page.ui.t("未发现可读取设备", "No readable device found")
                            color: device.name ? page.ui.text : page.ui.muted
                            font.pixelSize: page.ui.fontSmall
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            implicitWidth: 6
                            implicitHeight: 6
                            radius: page.ui.radiusPill
                            color: device.name ? page.ui.green : page.ui.borderStrong
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: page.ui.borderWidth
                        color: page.ui.borderSubtle
                        Layout.topMargin: page.ui.space1
                    }
                }
            }
            ActionButton {
                ui: page.ui
                text: page.ui.t("浏览完整清单", "Explore inventory")
                symbol: "arrow"
                compact: true
                Layout.fillWidth: true
                onClicked: page.appWindow.navigateTo(1)
            }
        }
    }
}
