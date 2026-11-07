pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button {
    id: control
    required property var ui
    property string iconName: "overview"
    property bool selected: false
    property bool compact: false
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: compact ? ui.controlHeightCompact : 44
    leftPadding: ui.space4 + ui.space1
    rightPadding: ui.space3
    font.pixelSize: ui.fontBodyLarge
    font.family: ui.fontFamily
    Accessible.name: text
    contentItem: RowLayout {
        spacing: control.ui.space3
        Icon {
            name: control.iconName
            color: control.selected ? control.ui.sidebarTextStrong : control.ui.sidebarText
            implicitWidth: 18
            implicitHeight: 18
            Behavior on color { ColorAnimation { duration: control.ui.durationFast } }
        }
        Label {
            text: control.text
            color: control.selected ? control.ui.sidebarTextStrong : control.ui.sidebarText
            font.pixelSize: control.font.pixelSize
            font.family: control.font.family
            font.weight: control.selected ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            Layout.fillWidth: true
            Behavior on color { ColorAnimation { duration: control.ui.durationFast } }
        }
    }
    background: Rectangle {
        radius: control.ui.radiusMd
        color: control.selected ? control.ui.sidebarActive
             : control.hovered ? control.ui.sidebarHover : "transparent"
        border.width: control.activeFocus ? control.ui.focusWidth : 0
        border.color: control.ui.focusRing
        Behavior on color { ColorAnimation { duration: control.ui.durationBase } }
        Rectangle {
            x: 0
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: 18
            radius: control.ui.radiusPill
            color: control.ui.accent
            opacity: control.selected ? 1 : 0
            scale: control.selected ? 1 : 0.5
            Behavior on opacity { NumberAnimation { duration: control.ui.durationSlow; easing.type: control.ui.easingStandard } }
            Behavior on scale { NumberAnimation { duration: control.ui.durationSlow; easing.type: control.ui.easingStandard } }
        }
    }
}
