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
    Layout.bottomMargin: 4
    spacing: 16
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 7
        Label { text: header.eyebrow; color: header.ui.muted; font.pixelSize: 10; font.letterSpacing: 1.5 }
        Label { text: header.title; color: header.ui.text; font.pixelSize: 27; font.weight: Font.DemiBold }
        Label { text: header.description; color: header.ui.secondary; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    RowLayout { id: actionsRow; spacing: 8 }
}
