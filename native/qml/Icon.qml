pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

Item {
    id: icon
    property string name: "overview"
    property color color: "#aaa4c2"
    property real strokeWidth: 1.6
    implicitWidth: 20
    implicitHeight: 20
    readonly property var paths: ({
        "overview": "M3 3H10V10H3Z M14 3H21V10H14Z M3 14H10V21H3Z M14 14H21V21H14Z",
        "hardware": "M6 6H18V18H6Z M9 9H15V15H9Z M9 2V6 M15 2V6 M9 18V22 M15 18V22 M2 9H6 M2 15H6 M18 9H22 M18 15H22",
        "sensors": "M2 12H6L9 4L14 20L17 12H22",
        "stress": "M13 2L4 14H11L10 22L20 9H13Z",
        "benchmark": "M4 20V12H8V20 M10 20V4H14V20 M16 20V8H20V20 M2 21H22",
        "report": "M5 2H14L20 8V22H5Z M14 2V8H20 M8 12H16 M8 16H16",
        "settings": "M9 3H15L16 7L20 8L22 13L18 16L17 21H11L9 17L5 16L3 11L7 8Z M9 12A3 3 0 1 0 15 12A3 3 0 1 0 9 12",
        "refresh": "M20 8A9 9 0 1 0 21 15 M20 3V8H15",
        "arrow": "M5 12H19 M13 6L19 12L13 18",
        "check": "M5 12L10 17L20 6",
        "star": "M12 2L15 8L22 9L17 14L18 21L12 18L6 21L7 14L2 9L9 8Z",
        "sun": "M8 12A4 4 0 1 0 16 12A4 4 0 1 0 8 12 M12 1V4 M12 20V23 M1 12H4 M20 12H23 M4 4L6 6 M18 18L20 20 M4 20L6 18 M18 6L20 4",
        "moon": "M20 14A8 8 0 0 1 10 4A8 8 0 1 0 20 14Z",
        "search": "M3 10A7 7 0 1 0 17 10A7 7 0 1 0 3 10 M15 15L22 22",
        "info": "M2 12A10 10 0 1 0 22 12A10 10 0 1 0 2 12 M12 11V17 M12 7V8",
        "warning": "M12 3L22 20H2Z M12 9V14 M12 17V18",
        "clock": "M2 12A10 10 0 1 0 22 12A10 10 0 1 0 2 12 M12 6V12L16 15",
        "copy": "M8 8H20V20H8Z M4 16V4H16",
        "chevronDown": "M6 9L12 15L18 9",
        "chevronUp": "M6 15L12 9L18 15",
        "pause": "M8 5V19 M16 5V19",
        "stop": "M5 5H19V19H5Z",
        "play": "M7 3L21 12L7 21Z"
    })
    Shape {
        width: 24
        height: 24
        scale: Math.min(icon.width, icon.height) / 24
        transformOrigin: Item.TopLeft
        antialiasing: true
        ShapePath {
            strokeColor: icon.color
            strokeWidth: icon.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: icon.paths[icon.name] || icon.paths.hardware }
        }
    }
}
