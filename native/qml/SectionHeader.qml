pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: header
    required property var ui
    property string title
    property string detail: ""
    property string overline: ""
    default property alias actions: trailing.data
    Layout.fillWidth: true
    spacing: ui.space3
    ColumnLayout {
        Layout.fillWidth: true
        spacing: ui.space1
        Label {
            text: header.overline
            visible: text !== ""
            color: header.ui.muted
            font.pixelSize: header.ui.fontMicro
            font.weight: Font.Medium
            font.letterSpacing: 1.2
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: header.ui.space2
            Label {
                text: header.title
                color: header.ui.text
                font.pixelSize: header.ui.fontSubtitle
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: header.detail
                visible: text !== ""
                color: header.ui.muted
                font.pixelSize: header.ui.fontCaption
                elide: Text.ElideRight
                Layout.maximumWidth: 360
            }
        }
    }
    RowLayout {
        id: trailing
        spacing: header.ui.space2
        Layout.alignment: Qt.AlignVCenter
    }
}
