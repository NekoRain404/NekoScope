pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: panel
    required property var ui
    property int padding: 22
    default property alias contentData: body.data
    property alias spacing: body.spacing
    color: ui.panel
    border.color: ui.border
    radius: 13
    implicitHeight: body.implicitHeight + padding * 2
    implicitWidth: body.implicitWidth + padding * 2
    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: panel.padding
        spacing: 16
    }
}
