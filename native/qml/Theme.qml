pragma ComponentBehavior: Bound
import QtQuick

QtObject {
    id: theme
    property bool dark: preferences.theme === "dark" || (preferences.theme === "system" && preferences.systemDark)
    property bool chinese: preferences.language !== "en"
    property color accent: preferences.accent
    property bool reducedMotion: preferences.reducedMotion
    property color background: dark ? "#171822" : "#f7f8fb"
    property color panel: dark ? "#20212e" : "#ffffff"
    property color soft: dark ? "#282938" : "#f5f5fa"
    property color border: dark ? "#333443" : "#e9eaf1"
    property color text: dark ? "#eeedf6" : "#292b39"
    property color secondary: dark ? "#b1b1c5" : "#747687"
    property color muted: dark ? "#9294ac" : "#9597a8"
    property color green: dark ? "#82c5aa" : "#408b72"
    property color warning: dark ? "#e5b581" : "#a56f38"
    property color accentSoft: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.17 : 0.10)
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
