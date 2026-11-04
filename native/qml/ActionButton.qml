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
    padding: compact ? ui.space2 : ui.space3
    leftPadding: compact ? ui.space3 : ui.space4
    rightPadding: leftPadding
    implicitHeight: compact ? ui.controlHeightCompact : ui.controlHeight
    font.pixelSize: ui.fontBody
    font.weight: primary ? Font.DemiBold : Font.Medium
    Accessible.name: text
    opacity: enabled ? 1 : 0.42
    contentItem: RowLayout {
        spacing: ui.space2
        Icon {
            visible: control.symbol !== ""
            name: control.symbol
            color: label.color
            implicitWidth: 16
            implicitHeight: 16
        }
        Label {
            id: label
            text: control.text
            color: control.primary ? control.ui.accentText
                 : control.danger ? control.ui.danger
                 : control.ui.text
            font: control.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
    background: Rectangle {
        id: surface
        radius: control.ui.radiusMd
        color: control.primary
             ? control.down ? Qt.darker(control.ui.accent, 1.16)
             : control.hovered ? Qt.lighter(control.ui.accent, 1.08)
             : control.ui.accent
             : control.danger
             ? control.down ? Qt.darker(control.ui.dangerSoft, 1.08)
             : control.hovered ? control.ui.dangerSoft
             : control.ui.panel
             : control.down ? Qt.darker(control.ui.soft, 1.06)
             : control.hovered ? control.ui.soft
             : control.ui.panel
        border.width: control.ui.borderWidth
        border.color: control.primary
                    ? control.hovered ? Qt.lighter(control.ui.accent, 1.30) : "transparent"
                    : control.danger
                    ? control.hovered || control.down ? control.ui.danger : control.ui.border
                    : control.hovered ? control.ui.borderStrong : control.ui.border
        Behavior on color { ColorAnimation { duration: control.ui.durationFast } }
        Behavior on border.color { ColorAnimation { duration: control.ui.durationFast } }
        // Offset focus-visible ring, so primary accent fills still show keyboard focus.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: surface.radius + 3
            color: "transparent"
            border.width: control.ui.focusWidth
            border.color: control.ui.focusRing
            visible: control.visualFocus && control.enabled
        }
    }
}
