pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

Column {
    id: empty
    required property var ui
    property string icon: "info"
    property string title: ""
    property string message: ""
    spacing: ui.space2
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        implicitWidth: empty.ui.space9
        implicitHeight: empty.ui.space9
        radius: empty.ui.radiusLg
        color: empty.ui.soft
        Icon {
            anchors.centerIn: parent
            name: empty.icon
            color: empty.ui.muted
            implicitWidth: 18
            implicitHeight: 18
        }
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        text: empty.title
        visible: text !== ""
        color: empty.ui.text
        font.pixelSize: empty.ui.fontBodyLarge
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
    }
    Label {
        width: empty.width
        text: empty.message
        visible: text !== ""
        color: empty.ui.muted
        font.pixelSize: empty.ui.fontSmall
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
