pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: chip
    required property var ui
    property string text
    property color tone: ui.secondary
    implicitWidth: row.implicitWidth + ui.space3
    implicitHeight: 24
    radius: ui.radiusPill
    color: Qt.rgba(chip.tone.r, chip.tone.g, chip.tone.b, chip.ui.dark ? 0.16 : 0.10)
    border.width: chip.ui.borderWidth
    border.color: Qt.rgba(chip.tone.r, chip.tone.g, chip.tone.b, 0.28)
    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: chip.ui.space2
        Rectangle {
            implicitWidth: 6
            implicitHeight: 6
            radius: chip.ui.radiusPill
            color: chip.tone
        }
        Label {
            text: chip.text
            color: chip.tone
            font.pixelSize: chip.ui.fontCaption
            font.weight: Font.Medium
        }
    }
}
