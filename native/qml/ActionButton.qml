pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button {
    id: control
    required property var ui
    property bool primary: false
    property bool danger: false
    property string symbol: ""
    property bool compact: false
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    padding: compact ? 9 : 12
    leftPadding: compact ? 12 : 16
    rightPadding: leftPadding
    implicitHeight: compact ? 34 : 40
    font.pixelSize: 12
    Accessible.name: text
    opacity: enabled ? 1 : 0.42
    contentItem: RowLayout {
        spacing: 8
        Icon { visible: control.symbol !== ""; name: control.symbol; color: label.color; implicitWidth: 16; implicitHeight: 16 }
        Label {
            id: label
            text: control.text
            color: control.primary ? "white" : (control.danger ? control.ui.warning : control.ui.text)
            font: control.font
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }
    background: Rectangle {
        radius: 8
        color: control.primary ? control.ui.accent : (control.hovered ? control.ui.soft : control.ui.panel)
        border.color: control.activeFocus ? control.ui.accent : (control.primary ? "transparent" : control.ui.border)
        border.width: control.activeFocus ? 2 : 1
        opacity: control.down ? 0.7 : 1
        Behavior on color { ColorAnimation { duration: control.ui.reducedMotion ? 0 : 120 } }
    }
}
