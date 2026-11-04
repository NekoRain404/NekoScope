pragma ComponentBehavior: Bound
import QtQuick

QtObject {
    id: theme

    // Preferences-backed public contract (used by pages and shell).
    property bool dark: preferences.theme === "dark" || (preferences.theme === "system" && preferences.systemDark)
    property bool chinese: preferences.language !== "en"
    property color accent: preferences.accent
    property bool reducedMotion: preferences.reducedMotion

    // Semantic surfaces.
    property color background: dark ? "#151620" : "#f6f7fa"
    property color panel: dark ? "#1e1f2b" : "#ffffff"
    property color soft: dark ? "#252735" : "#f3f4f9"
    property color border: dark ? "#30323f" : "#e6e8f0"
    property color text: dark ? "#eeedf6" : "#242634"
    property color secondary: dark ? "#b0b1c3" : "#6d7082"
    property color muted: dark ? "#8e90a6" : "#9497a8"
    property color green: dark ? "#82c5aa" : "#3f8a71"
    property color warning: dark ? "#e5b581" : "#a06c36"
    property color accentSoft: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.17 : 0.10)

    // Extended semantic palette.
    readonly property color accentText: "#ffffff"
    readonly property color danger: dark ? "#e39a9a" : "#b05252"
    readonly property color borderStrong: dark ? "#424457" : "#d7d9e4"
    readonly property color borderSubtle: dark ? "#292b38" : "#eef0f5"
    readonly property color positiveSoft: Qt.rgba(green.r, green.g, green.b, dark ? 0.16 : 0.10)
    readonly property color warningSoft: Qt.rgba(warning.r, warning.g, warning.b, dark ? 0.16 : 0.10)
    readonly property color dangerSoft: Qt.rgba(danger.r, danger.g, danger.b, dark ? 0.16 : 0.10)
    readonly property color focusRing: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.55 : 0.38)
    readonly property color chartGrid: dark ? "#2a2c3a" : "#eceef4"
    readonly property color documentText: "#242634"
    readonly property color documentBackground: "#ffffff"

    // Hero surface (intentionally dark in both themes for contrast).
    readonly property color heroTop: "#2a2838"
    readonly property color heroBottom: "#211f2e"
    readonly property color heroLine: "#4b4460"
    readonly property color heroText: "#f6f2ff"
    readonly property color heroSecondary: "#b8b2cb"
    readonly property color heroMuted: "#9991af"

    // Navigation rail (dark material in both themes).
    readonly property color sidebarTop: "#20212f"
    readonly property color sidebarBottom: "#1a1b26"
    readonly property color sidebarHover: "#2c2d3f"
    readonly property color sidebarActive: "#363244"
    readonly property color sidebarBorder: "#333245"
    readonly property color sidebarText: "#aaa7bd"
    readonly property color sidebarTextStrong: "#f3f0ff"
    readonly property color sidebarMuted: "#7c7a92"

    // Type scale.
    readonly property int fontMicro: 9
    readonly property int fontCaption: 10
    readonly property int fontSmall: 11
    readonly property int fontBody: 12
    readonly property int fontBodyLarge: 13
    readonly property int fontSubtitle: 15
    readonly property int fontTitle: 17
    readonly property int fontHeading: 22
    readonly property int fontDisplay: 28
    readonly property int fontMetric: 30
    readonly property string fontFamily: Qt.application.font.family || "Noto Sans"
    readonly property string monoFamily: "monospace"

    // 4 px spacing scale.
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 20
    readonly property int space6: 24
    readonly property int space7: 28
    readonly property int space8: 32
    readonly property int space9: 40

    // Radii scale.
    readonly property int radiusXs: 4
    readonly property int radiusSm: 6
    readonly property int radiusMd: 8
    readonly property int radiusLg: 12
    readonly property int radiusXl: 16
    readonly property int radiusPill: 999

    // Borders, controls and elevation.
    readonly property int borderWidth: 1
    readonly property int focusWidth: 2
    readonly property int controlHeight: 38
    readonly property int controlHeightCompact: 34
    readonly property int elevationFlat: 0
    readonly property int elevationRaised: 1
    readonly property int elevationFloating: 2

    // Motion: durations collapse to zero when reduced motion is requested.
    readonly property int durationFast: reducedMotion ? 0 : 120
    readonly property int durationBase: reducedMotion ? 0 : 180
    readonly property int durationSlow: reducedMotion ? 0 : 280
    readonly property int durationLong: reducedMotion ? 0 : 460
    readonly property int durationPulse: reducedMotion ? 0 : 900
    readonly property int easingStandard: Easing.OutCubic
    readonly property int easingDecelerate: Easing.OutQuint

    function t(zh, en) { return chinese ? zh : en }
    function metric(value, digits, allowNegative) {
        return value !== undefined && value !== null && Number.isFinite(Number(value)) && (allowNegative || Number(value) >= 0)
                ? Number(value).toFixed(digits === undefined ? 1 : digits) : "—"
    }
    function status(value) {
        const names = {
            "available": ["可用", "Available"], "unavailable": ["不可用", "Unavailable"],
            "permission-denied": ["需要权限", "Permission required"], "unsupported": ["不支持", "Unsupported"],
            "offline": ["设备离线", "Offline"], "stale": ["数据已过期", "Stale"],
            "error": ["读取失败", "Read error"], "partial": ["部分可用", "Partially available"],
            "idle": ["准备就绪", "Ready"], "starting": ["正在启动", "Starting"],
            "running": ["正在运行", "Running"], "stopping": ["正在停止", "Stopping"],
            "completed": ["已完成", "Completed"], "cancelled": ["已取消", "Cancelled"],
            "failed": ["失败", "Failed"], "thermal-stop": ["温度保护停止", "Thermal stop"]
        }
        return names[value] ? t(names[value][0], names[value][1]) : (value || t("未知", "Unknown"))
    }
    function category(value) {
        const names = {"CPU": ["处理器", "Processor"], "内存": ["内存", "Memory"],
            "主板/BIOS": ["主板与固件", "Board & firmware"], "存储": ["存储", "Storage"],
            "显卡": ["显卡", "Graphics"], "网络": ["网络", "Network"],
            "显示器": ["显示器", "Displays"], "音频": ["音频", "Audio"],
            "电池": ["电池", "Battery"], "电源": ["电源", "Power"], "系统": ["系统", "System"]}
        return names[value] ? t(names[value][0], names[value][1]) : value
    }
    function capability(name) {
        for (const item of testRunner.capabilities) if (item.name === name) return item.available
        return false
    }
}
