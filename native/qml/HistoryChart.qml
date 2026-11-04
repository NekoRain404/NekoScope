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
    property real reveal: 1
    implicitHeight: 210
    Accessible.role: Accessible.Chart
    Accessible.name: ui.t("实时历史曲线", "Live history chart")
    readonly property var validSamples: samples.filter(value => Number.isFinite(Number(value)) && Number(value) >= 0)
    readonly property real upperBound: Math.max(maximum, ...validSamples, 1)
    function points() {
        const result = []
        const values = validSamples
        for (let i = 0; i < values.length; ++i)
            result.push(Qt.point(i * plot.width / Math.max(values.length - 1, 1),
                                 plot.height * (1 - Math.max(0, values[i]) / upperBound)))
        return result
    }
    function areaPoints() {
        const line = points()
        if (line.length < 2) return []
        return [Qt.point(0, plot.height), ...line, Qt.point(plot.width, plot.height)]
    }
    Repeater {
        model: 5
        delegate: Item {
            required property int index
            x: 0
            y: 8 + index * (chart.height - 34) / 4
            width: chart.width
            height: 1
            Label {
                width: 34
                text: Math.round(chart.upperBound * (1 - index / 4))
                color: chart.ui.muted
                font.pixelSize: chart.ui.fontMicro
                y: -6
            }
            Rectangle {
                x: 38
                width: Math.max(0, parent.width - 38)
                height: 1
                color: chart.ui.chartGrid
            }
        }
    }
    Label {
        anchors.right: parent.right
        y: 0
        text: chart.unit
        color: chart.ui.muted
        font.pixelSize: chart.ui.fontMicro
    }
    Item {
        id: plot
        anchors { left: parent.left; leftMargin: 38; right: parent.right; top: parent.top; topMargin: 8; bottom: parent.bottom; bottomMargin: 26 }
        clip: true
        Item {
            id: revealMask
            width: plot.width * chart.reveal
            height: plot.height
            clip: true
            Shape {
                width: plot.width
                height: plot.height
                visible: chart.validSamples.length > 1
                antialiasing: true
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: LinearGradient {
                        x1: 0
                        y1: 0
                        x2: 0
                        y2: plot.height
                        GradientStop {
                            position: 0
                            color: Qt.rgba(chart.lineColor.r, chart.lineColor.g, chart.lineColor.b, chart.ui.dark ? 0.18 : 0.12)
                        }
                        GradientStop {
                            position: 1
                            color: Qt.rgba(chart.lineColor.r, chart.lineColor.g, chart.lineColor.b, 0)
                        }
                    }
                    PathPolyline { path: chart.areaPoints() }
                }
                ShapePath {
                    strokeColor: chart.lineColor
                    strokeWidth: 2.5
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathPolyline { path: chart.points() }
                }
            }
        }
        Label {
            anchors.centerIn: parent
            visible: chart.validSamples.length < 2
            text: chart.ui.t("等待采集记录…", "Waiting for samples…")
            color: chart.ui.muted
            font.pixelSize: chart.ui.fontBody
        }
    }
    Label {
        x: 38
        anchors.bottom: parent.bottom
        text: ui.t("较早", "Earlier")
        color: ui.muted
        font.pixelSize: ui.fontMicro
    }
    Label {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: ui.t("现在", "Now") + " · " + validSamples.length + ui.t(" 个样本", " samples")
        color: ui.muted
        font.pixelSize: ui.fontMicro
    }
    NumberAnimation {
        id: revealAnimation
        target: chart
        property: "reveal"
        from: 0
        to: 1
        duration: chart.ui.durationLong
        easing.type: chart.ui.easingDecelerate
    }
    Component.onCompleted: if (!chart.ui.reducedMotion) revealAnimation.restart()
    onVisibleChanged: if (visible && !chart.ui.reducedMotion) revealAnimation.restart()
}
