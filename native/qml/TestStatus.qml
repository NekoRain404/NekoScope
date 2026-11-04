pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Panel {
    id: panel
    property string kind: "stress"
    readonly property bool relevant: testRunner.state.kind === kind
    readonly property var runState: relevant ? testRunner.state : ({status: "idle", running: false, progress: 0, elapsed: 0, duration: 0, message: "", output: ""})
    spacing: 18
    RowLayout {
        Layout.fillWidth: true
        Rectangle {
            implicitWidth: 40; implicitHeight: 40; radius: 10; color: panel.ui.accentSoft
            Icon { anchors.centerIn: parent; name: panel.runState.running ? "sensors" : "play"; color: panel.ui.accent; width: 20; height: 20 }
        }
        ColumnLayout {
            Layout.fillWidth: true; spacing: 5
            Label { text: panel.ui.status(panel.runState.status); color: panel.ui.text; font.pixelSize: 15; font.weight: Font.DemiBold; objectName: panel.kind + "StatusLabel" }
            Label { text: panel.runState.running ? panel.ui.t("可随时停止，切换页面不会中断测试。", "Stop at any time. Navigation keeps the test running.") : panel.ui.t("仅在你启动后施加负载。", "Load begins only when you start a test."); color: panel.ui.muted; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
        ActionButton { ui: panel.ui; visible: panel.runState.running; danger: true; text: panel.ui.t("停止", "Stop"); symbol: "stop"; onClicked: testRunner.stop(); objectName: panel.kind + "StopButton" }
    }
    ProgressBar { from: 0; to: 100; value: panel.runState.progress || 0; Layout.fillWidth: true }
    RowLayout {
        Layout.fillWidth: true
        Label { text: panel.ui.t("已用时间", "Elapsed") + "  " + (panel.runState.elapsed || 0) + " s"; color: panel.ui.secondary; font.pixelSize: 11 }
        Item { Layout.fillWidth: true }
        Label { text: panel.ui.t("目标时长", "Duration") + "  " + (panel.runState.duration || 0) + " s"; color: panel.ui.muted; font.pixelSize: 11 }
    }
    Label { visible: panel.runState.score !== undefined && panel.runState.score !== null && panel.runState.status === "completed"; text: panel.ui.metric(panel.runState.score, 2) + " " + (panel.runState.unit || ""); color: panel.ui.accent; font.pixelSize: 27; font.weight: Font.DemiBold }
    Label { text: panel.ui.t("运行诊断", "RUN DIAGNOSTICS"); color: panel.ui.muted; font.pixelSize: 9; font.letterSpacing: 1.2 }
    Label { text: panel.runState.message || panel.ui.t("等待测试启动。", "Waiting for a test to start."); color: panel.runState.status === "failed" ? panel.ui.warning : panel.ui.secondary; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    ScrollView {
        Layout.fillWidth: true; Layout.preferredHeight: 116
        clip: true
        TextArea {
            readOnly: true; selectByMouse: true; text: panel.runState.output || panel.ui.t("工具原始输出将在此显示。", "Raw tool output will appear here.")
            color: panel.ui.secondary; font.family: "monospace"; font.pixelSize: 10; wrapMode: TextEdit.WrapAnywhere
            background: Rectangle { color: panel.ui.soft; radius: 7 }
        }
    }
}
