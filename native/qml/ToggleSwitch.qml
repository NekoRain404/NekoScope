pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

Switch {
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
        implicitWidth: 40
        implicitHeight: 22
        radius: control.ui.radiusPill
        color: control.checked ? control.ui.accent : control.ui.soft
        border.width: control.activeFocus ? control.ui.focusWidth : control.ui.borderWidth
        border.color: control.activeFocus ? control.ui.focusRing
                    : control.checked ? control.ui.accent : control.ui.border
        Behavior on color { ColorAnimation { duration: control.ui.durationBase } }
        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            implicitWidth: 16
            implicitHeight: 16
            radius: control.ui.radiusPill
            color: control.checked ? control.ui.accentText : control.ui.muted
            Behavior on x {
                NumberAnimation {
                    duration: control.ui.durationBase
                    easing.type: control.ui.easingStandard
                }
            }
        }
    }
}
