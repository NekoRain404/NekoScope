pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: surface
    required property var ui
    required property var owner
    radius: ui.radiusSm
    color: !owner.enabled ? ui.soft
         : owner.activeFocus ? ui.panel
         : owner.hovered ? ui.panel : ui.soft
    border.width: owner.activeFocus ? ui.focusWidth : ui.borderWidth
    border.color: owner.activeFocus ? ui.accent
                : owner.hovered ? ui.borderStrong : ui.border
    Behavior on color { ColorAnimation { duration: surface.ui.durationFast } }
    Behavior on border.color { ColorAnimation { duration: surface.ui.durationFast } }
}
