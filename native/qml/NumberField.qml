pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

SpinBox {
    id: control
    required property var ui
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: ui.controlHeight
    leftPadding: ui.space3
    rightPadding: ui.space8 + ui.space1
    font.pixelSize: ui.fontBody
    font.family: ui.fontFamily
    contentItem: TextInput {
        z: 2
        text: control.displayText
        font: control.font
        color: control.enabled ? control.ui.text : control.ui.muted
        selectionColor: control.ui.accent
        selectedTextColor: control.ui.accentText
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: TextInput.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
        selectByMouse: true
        leftPadding: control.leftPadding
        rightPadding: control.rightPadding
    }
    up.indicator: Rectangle {
        x: control.width - width - 3
        y: 3
        width: 22
        height: (control.height - 6) / 2
        radius: control.ui.radiusXs
        color: control.up.pressed ? control.ui.soft : "transparent"
        Icon {
            anchors.centerIn: parent
            name: "chevronUp"
            color: control.enabled ? control.ui.muted : control.ui.borderStrong
            implicitWidth: 10
            implicitHeight: 10
        }
    }
    down.indicator: Rectangle {
        x: control.width - width - 3
        y: control.height / 2
        width: 22
        height: (control.height - 6) / 2
        radius: control.ui.radiusXs
        color: control.down.pressed ? control.ui.soft : "transparent"
        Icon {
            anchors.centerIn: parent
            name: "chevronDown"
            color: control.enabled ? control.ui.muted : control.ui.borderStrong
            implicitWidth: 10
            implicitHeight: 10
        }
    }
    background: FieldSurface { ui: control.ui; owner: control }
}
