pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: field
    required property var ui
    hoverEnabled: true
    implicitHeight: ui.controlHeight
    leftPadding: ui.space9
    rightPadding: ui.space3
    font.pixelSize: ui.fontBody
    font.family: ui.fontFamily
    color: enabled ? ui.text : ui.muted
    placeholderTextColor: ui.muted
    selectionColor: ui.accent
    selectedTextColor: ui.accentText
    verticalAlignment: TextInput.AlignVCenter
    Accessible.name: placeholderText
    background: FieldSurface { ui: field.ui; owner: field }
    Icon {
        anchors.left: parent.left
        anchors.leftMargin: field.ui.space3
        anchors.verticalCenter: parent.verticalCenter
        z: 1
        name: "search"
        color: field.enabled ? field.ui.muted : field.ui.border
        implicitWidth: 15
        implicitHeight: 15
    }
}
