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
    padding: ui.space5
    spacing: ui.space3
    Layout.fillWidth: true
    Label {
        text: card.title
        color: card.ui.secondary
        font.pixelSize: card.ui.fontCaption
        font.weight: Font.Medium
        font.letterSpacing: 0.2
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: card.ui.space2
        Label {
            text: card.value
            color: card.ui.text
            font.pixelSize: card.ui.fontMetric
            font.weight: Font.DemiBold
        }
        Label {
            text: card.unit
            color: card.ui.muted
            font.pixelSize: card.ui.fontBody
            Layout.alignment: Qt.AlignBaseline
        }
        Item { Layout.fillWidth: true }
        Rectangle {
            implicitWidth: card.ui.space6
            implicitHeight: card.ui.space6
            radius: card.ui.radiusPill
            color: Qt.rgba(card.tone.r, card.tone.g, card.tone.b, card.ui.dark ? 0.18 : 0.12)
            Rectangle {
                anchors.centerIn: parent
                implicitWidth: 7
                implicitHeight: 7
                radius: card.ui.radiusPill
                color: card.tone
            }
        }
    }
    Label {
        text: card.detail
        color: card.ui.muted
        font.pixelSize: card.ui.fontCaption
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
}
