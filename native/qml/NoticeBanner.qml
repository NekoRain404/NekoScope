pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: banner
    required property var ui
    property string text
    property string icon: "warning"
    property color tone: ui.warning
    property color surfaceColor: ui.warningSoft
    property bool compact: false
    property alias textObjectName: textLabel.objectName
    implicitHeight: bannerRow.implicitHeight + (compact ? ui.space3 : ui.space4)
    radius: ui.radiusSm
    color: surfaceColor
    border.width: ui.borderWidth
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.30)
    RowLayout {
        id: bannerRow
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            margins: banner.compact ? banner.ui.space2 : banner.ui.space3
        }
        spacing: banner.ui.space2
        Icon {
            name: banner.icon
            color: banner.tone
            implicitWidth: banner.compact ? 14 : 16
            implicitHeight: banner.compact ? 14 : 16
        }
        Label {
            id: textLabel
            text: banner.text
            color: banner.tone
            font.pixelSize: banner.compact ? banner.ui.fontCaption : banner.ui.fontSmall
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
