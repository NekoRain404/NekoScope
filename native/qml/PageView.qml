pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Flickable {
    id: page
    required property var ui
    default property alias contentData: body.data
    contentWidth: width
    contentHeight: body.implicitHeight + 60
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    ColumnLayout {
        id: body
        x: page.width > 1100 ? 40 : 28
        y: 28
        width: page.width - x * 2
        spacing: 22
    }
}
