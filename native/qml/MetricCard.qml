pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Panel {
    id: card
    property string title
    property string value
    property string unit
    property string detail
    property color tone: ui.accent
    padding: 20
    spacing: 12
    Layout.fillWidth: true
    Label { text: card.title; color: card.ui.secondary; font.pixelSize: 12 }
    RowLayout {
        spacing: 6
        Label { text: card.value; color: card.ui.text; font.pixelSize: 30; font.weight: Font.DemiBold }
        Label { text: card.unit; color: card.ui.muted; font.pixelSize: 13; Layout.alignment: Qt.AlignBaseline }
        Item { Layout.fillWidth: true }
        Rectangle { implicitWidth: 7; implicitHeight: 7; radius: 4; color: card.tone }
    }
    Label { text: card.detail; color: card.ui.muted; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
}
