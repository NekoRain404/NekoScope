pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: control
    required property var ui
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: ui.controlHeight
    leftPadding: ui.space3
    rightPadding: ui.space3
    font.pixelSize: ui.fontBody
    font.family: ui.fontFamily
    contentItem: Label {
        leftPadding: control.leftPadding
        rightPadding: control.rightPadding + 22
        text: control.displayText
        font: control.font
        color: control.enabled ? control.ui.text : control.ui.muted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Icon {
        x: control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        name: "chevronDown"
        color: control.enabled ? control.ui.muted : control.ui.border
        implicitWidth: 13
        implicitHeight: 13
    }
    background: FieldSurface { ui: control.ui; owner: control }
}
