pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Flickable {
    id: page
    required property var ui
    default property alias contentData: body.data
    readonly property int contentTop: ui.space7
    readonly property int sideInset: width > 1100 ? ui.space9 : ui.space7
    property real enterOffset: 0
    contentWidth: width
    contentHeight: body.implicitHeight + contentTop + ui.space8
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    function playEnter() {
        if (ui.reducedMotion) {
            page.opacity = 1
            page.enterOffset = 0
            return
        }
        enterTransition.restart()
    }
    Component.onCompleted: playEnter()
    onVisibleChanged: if (visible && enterTransition) playEnter()
    ColumnLayout {
        id: body
        x: page.sideInset
        y: page.contentTop + page.enterOffset
        width: page.width - x * 2
        spacing: page.ui.space5
    }
    ParallelAnimation {
        id: enterTransition
        NumberAnimation {
            target: page
            property: "opacity"
            from: 0
            to: 1
            duration: page.ui.durationBase
            easing.type: page.ui.easingStandard
        }
        NumberAnimation {
            target: page
            property: "enterOffset"
            from: 6
            to: 0
            duration: page.ui.durationBase
            easing.type: page.ui.easingStandard
        }
    }
}
