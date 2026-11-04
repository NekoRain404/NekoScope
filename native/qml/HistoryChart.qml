pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Shapes

Item {
    id: chart
    required property var ui
    property var samples: []
    property color lineColor: ui.accent
    property real maximum: 100
    property string unit: "%"
    implicitHeight: 210
    Accessible.role: Accessible.Chart
    Accessible.name: ui.t("实时历史曲线", "Live history chart")
    readonly property var validSamples: samples.filter(value => Number.isFinite(Number(value)) && Number(value) >= 0)
    function points() {
        const result = []
        const values = validSamples
        const upper = Math.max(maximum, ...values, 1)
        for (let i = 0; i < values.length; ++i)
            result.push(Qt.point(i * plot.width / Math.max(values.length - 1, 1), plot.height * (1 - Math.max(0, values[i]) / upper)))
        return result
    }
    Repeater {
        model: 5
        delegate: Item {
            required property int index
            x: 0; y: 8 + index * (chart.height - 34) / 4
            width: chart.width; height: 1
            Label { width: 34; text: Math.round(chart.maximum * (1 - index / 4)); color: chart.ui.muted; font.pixelSize: 9; y: -6 }
            Rectangle { x: 38; width: parent.width - 38; height: 1; color: chart.ui.border; opacity: 0.7 }
        }
    }
    Item {
        id: plot
        anchors { left: parent.left; leftMargin: 38; right: parent.right; top: parent.top; topMargin: 8; bottom: parent.bottom; bottomMargin: 26 }
        Shape {
            anchors.fill: parent
            visible: chart.validSamples.length > 1
            ShapePath {
                strokeColor: chart.lineColor
                strokeWidth: 2.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathPolyline { path: chart.points() }
            }
        }
        Label {
            anchors.centerIn: parent
            visible: chart.validSamples.length < 2
            text: chart.ui.t("等待采集记录…", "Waiting for samples…")
            color: chart.ui.muted; font.pixelSize: 12
        }
    }
    Label { x: 38; anchors.bottom: parent.bottom; text: ui.t("较早", "Earlier"); color: ui.muted; font.pixelSize: 9 }
    Label { anchors.right: parent.right; anchors.bottom: parent.bottom; text: ui.t("现在", "Now") + " · " + validSamples.length + ui.t(" 个样本", " samples"); color: ui.muted; font.pixelSize: 9 }
}
