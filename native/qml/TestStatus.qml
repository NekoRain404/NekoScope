pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Panel {
    id: panel
    property string kind: "stress"
    readonly property bool relevant: testRunner.state.kind === kind
    readonly property var runState: relevant ? testRunner.state : ({status: "idle", running: false, progress: 0, elapsed: 0, duration: 0, message: "", messageKey: "", messageParams: {}, output: ""})
    readonly property string statusMessage: {
        const translated = panel.ui.trKey(panel.runState.messageKey || "", panel.runState.messageParams, panel.runState.message || "")
        return translated || panel.ui.t("等待测试启动。", "Waiting for a test to start.")
    }
    readonly property color stateTone: panel.runState.status === "failed" || panel.runState.status === "thermal-stop" ? panel.ui.warning
                                           : panel.runState.status === "completed" ? panel.ui.green
                                           : panel.runState.running ? panel.ui.accent : panel.ui.secondary
    spacing: panel.ui.space4
    RowLayout {
        Layout.fillWidth: true
        spacing: panel.ui.space3
        Rectangle {
            implicitWidth: 42
            implicitHeight: 42
            radius: panel.ui.radiusLg
            color: panel.ui.accentSoft
            Icon {
                anchors.centerIn: parent
                name: panel.runState.running ? "sensors" : "play"
                color: panel.ui.accent
                implicitWidth: 20
                implicitHeight: 20
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: panel.ui.space1
            RowLayout {
                spacing: panel.ui.space2
                Label {
                    text: panel.ui.status(panel.runState.status)
                    color: panel.ui.text
                    font.pixelSize: panel.ui.fontTitle
                    font.weight: Font.DemiBold
                    objectName: panel.kind + "StatusLabel"
                }
                StatusChip {
                    ui: panel.ui
                    text: panel.runState.running ? panel.ui.t("进行中", "In progress") : panel.ui.t("空闲", "Idle")
                    tone: panel.stateTone
                    visible: panel.runState.running
                }
            }
            Label {
                text: panel.runState.running ? panel.ui.t("可随时停止，切换页面不会中断测试。", "Stop at any time. Navigation keeps the test running.")
                                                : panel.ui.t("仅在你启动后施加负载。", "Load begins only when you start a test.")
                color: panel.ui.muted
                font.pixelSize: panel.ui.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
        Rectangle {
            visible: panel.runState.running
            implicitWidth: 9
            implicitHeight: 9
            radius: panel.ui.radiusPill
            color: panel.ui.accent
            SequentialAnimation on opacity {
                running: panel.runState.running && !panel.ui.reducedMotion
                loops: Animation.Infinite
                NumberAnimation { from: 0.95; to: 0.30; duration: panel.ui.durationPulse; easing.type: Easing.InOutSine }
                NumberAnimation { from: 0.30; to: 0.95; duration: panel.ui.durationPulse; easing.type: Easing.InOutSine }
            }
        }
        ActionButton {
            ui: panel.ui
            visible: panel.runState.running
            danger: true
            text: panel.ui.t("停止", "Stop")
            symbol: "stop"
            onClicked: testRunner.stop()
            objectName: panel.kind + "StopButton"
        }
    }
    ProgressBar {
        id: progress
        from: 0
        to: 100
        value: panel.runState.progress || 0
        Layout.fillWidth: true
        implicitHeight: 6
        background: Rectangle {
            implicitHeight: 6
            radius: 3
            color: panel.ui.soft
        }
        contentItem: Item {
            Rectangle {
                width: Math.max(0, Math.min(1, progress.visualPosition)) * parent.width
                height: 6
                radius: 3
                color: panel.ui.accent
                opacity: panel.runState.running ? 1 : 0.55
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Label {
            text: panel.ui.t("已用时间", "Elapsed") + "  " + (panel.runState.elapsed || 0) + " s"
            color: panel.ui.secondary
            font.pixelSize: panel.ui.fontSmall
        }
        Item { Layout.fillWidth: true }
        Label {
            text: panel.ui.t("目标时长", "Duration") + "  " + (panel.runState.duration || 0) + " s"
            color: panel.ui.muted
            font.pixelSize: panel.ui.fontSmall
        }
    }
    Label {
        visible: panel.runState.score !== undefined && panel.runState.score !== null && panel.runState.status === "completed"
        text: panel.ui.metric(panel.runState.score, 2) + " " + (panel.runState.unit || "")
        color: panel.ui.accent
        font.pixelSize: panel.ui.fontHeading
        font.weight: Font.DemiBold
    }
    Label {
        text: panel.statusMessage
        color: panel.runState.status === "failed" ? panel.ui.warning : panel.ui.secondary
        font.pixelSize: panel.ui.fontSmall
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Label {
        text: panel.ui.t("工具原始输出", "TOOL OUTPUT")
        color: panel.ui.muted
        font.pixelSize: panel.ui.fontMicro
        font.weight: Font.Medium
        font.letterSpacing: 1.2
    }
    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: 116
        clip: true
        TextArea {
            readOnly: true
            selectByMouse: true
            text: panel.runState.output || panel.ui.t("工具原始输出将在此显示。", "Raw tool output will appear here.")
            color: panel.ui.secondary
            font.family: panel.ui.monoFamily
            font.pixelSize: panel.ui.fontCaption
            wrapMode: TextEdit.WrapAnywhere
            background: Rectangle { color: panel.ui.soft; radius: panel.ui.radiusSm }
        }
    }
}
