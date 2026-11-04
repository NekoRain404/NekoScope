pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: panel
    required property var ui
    property int padding: ui.space5
    property int cornerRadius: ui.radiusLg
    property int elevation: ui.elevationRaised
    property color fillColor: ui.panel
    default property alias contentData: body.data
    property alias spacing: body.spacing
    color: fillColor
    border.width: ui.borderWidth
    border.color: elevation >= ui.elevationFloating ? ui.borderStrong
                : elevation >= ui.elevationRaised ? ui.border : ui.borderSubtle
    radius: cornerRadius
    implicitHeight: body.implicitHeight + padding * 2
    implicitWidth: body.implicitWidth + padding * 2
    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: panel.padding
        spacing: panel.ui.space4
    }
}
