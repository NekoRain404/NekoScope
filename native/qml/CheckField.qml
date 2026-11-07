pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

CheckBox {
    id: control
    required property var ui
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    spacing: ui.space2
    font.pixelSize: ui.fontBody
    font.family: ui.fontFamily
    contentItem: Label {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: control.enabled ? control.ui.text : control.ui.muted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 18
        implicitHeight: 18
        radius: control.ui.radiusXs
        color: control.checked ? control.ui.accent : (control.hovered ? control.ui.soft : "transparent")
        border.width: control.activeFocus ? control.ui.focusWidth : control.ui.borderWidth
        border.color: control.activeFocus ? control.ui.focusRing
                    : control.checked ? control.ui.accent
                    : control.hovered ? control.ui.borderStrong : control.ui.border
        Behavior on color { ColorAnimation { duration: control.ui.durationFast } }
        Behavior on border.color { ColorAnimation { duration: control.ui.durationFast } }
        Icon {
            anchors.centerIn: parent
            name: "check"
            color: control.ui.accentText
            visible: control.checked
            implicitWidth: 13
            implicitHeight: 13
        }
    }
}
