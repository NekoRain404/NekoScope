pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: header
    required property var ui
    property string title
    property string description
    property string eyebrow: "NEKOSCOPE / INSIGHTS"
    default property alias actions: actionsRow.data
    Layout.fillWidth: true
    Layout.bottomMargin: ui.space2
    spacing: ui.space5
    ColumnLayout {
        Layout.fillWidth: true
        spacing: ui.space2
        Label {
            text: header.eyebrow
            visible: text !== ""
            color: header.ui.muted
            font.pixelSize: header.ui.fontCaption
            font.weight: Font.Medium
            font.letterSpacing: 1.5
        }
        Label {
            text: header.title
            color: header.ui.text
            font.pixelSize: header.ui.fontDisplay
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            text: header.description
            visible: text !== ""
            color: header.ui.secondary
            font.pixelSize: header.ui.fontBody
            lineHeight: 1.3
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
    RowLayout {
        id: actionsRow
        spacing: header.ui.space2
        Layout.alignment: Qt.AlignTop
    }
}
