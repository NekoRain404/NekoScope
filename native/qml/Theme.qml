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
            "failed": ["失败", "Failed"], "thermal-stop": ["温度保护停止", "Thermal stop"],
            "sensor-lost": ["传感器丢失", "Sensor lost"], "read-error": ["读取失败", "Read error"],
            "fresh": ["最新", "Fresh"]
        }
        return names[value] ? t(names[value][0], names[value][1]) : (value || t("未知", "Unknown"))
    }

    // Stable ASCII keys authored by backend services. Unknown keys fall back to
    // the raw diagnostic text so older records still render.
    readonly property var keyedMessages: ({
        "message.busy": ["已有测试正在运行", "A test is already running"],
        "message.preparing": ["正在准备测试", "Preparing the test"],
        "message.running": ["测试已启动", "Test started"],
        "reason.completed": ["测试完成", "Test completed"],
        "reason.stopped-by-user": ["用户取消", "Stopped by user"],
        "reason.thermal-stop": ["温度达到 90 °C，测试已停止", "Temperature reached 90 °C; the test was stopped"],
        "reason.thermal-stop-preflight": ["温度达到 90 °C，未启动测试", "Temperature reached 90 °C; the test was not started"],
        "reason.sensor-lost": ["温度传感器不可用，测试已停止", "Temperature sensor became unavailable; the test was stopped"],
        "reason.sensor-lost-stale": ["温度采集已过期，测试已停止", "Temperature readings became stale; the test was stopped"],
        "reason.timeout": ["测试超过时限，已终止", "The test exceeded its time limit and was terminated"],
        "reason.missing-tool": ["未找到 {tool}；请安装后重试", "Missing {tool}. Install it and try again."],
        "reason.memory-unavailable": ["无法读取可用内存，不能安全启动内存压力测试", "Available memory could not be read; the memory test cannot start safely"],
        "reason.insufficient-memory": ["可用内存不足以安全启动压力测试", "Not enough available memory to start the stress test safely"],
        "reason.launch-failed": ["无法启动测试工具：{error}", "The test tool could not be started: {error}"],
        "reason.process-failed": ["测试进程异常结束", "The test process ended unexpectedly"],
        "reason.unsupported-stress-type": ["不支持的压力测试类型", "Unsupported stress test type"],
        "reason.invalid-parameters": ["测试参数超出允许范围", "Test parameters are outside the allowed range"],
        "reason.unsupported-benchmark-type": ["不支持的基准测试类型", "Unsupported benchmark type"],
        "reason.invalid-duration": ["基准测试时长必须为 1 到 300 秒", "Benchmark duration must be between 1 and 300 seconds"],
        "reason.benchmark-unparsed": ["无法从基准输出解析有效成绩", "No valid score could be parsed from the benchmark output"],
        "reason.benchmark-invalid": ["基准成绩无效", "The benchmark score was invalid"],
        "reports.error.local-file": ["请选择本地文件", "Choose a local file"],
        "reports.error.unsupported-format": ["不支持的报告格式", "Unsupported report format"],
        "reports.error.write-failed": ["无法写入报告文件", "Unable to write the report file"]
    })
    function trKey(key, params, fallback) {
        const entry = key ? keyedMessages[key] : undefined
        if (!entry) return fallback === undefined || fallback === null ? (key || "") : String(fallback)
        let text = t(entry[0], entry[1])
        if (params) {
            for (const name of Object.keys(params))
                text = text.replace("{" + name + "}", String(params[name]))
        }
        return text
    }
    function message(key, params, fallback) { return trKey(key, params, fallback) }
    function reason(key, params, fallback) { return trKey(key, params, fallback) }
    // Inventory reasons are stable English tokens; translate known tokens and
    // show anything else verbatim rather than hiding the diagnostic.
    function fieldReason(value) {
        if (!value) return ""
        const names = {
            "read access denied": ["读取权限不足", "Read access denied"],
            "attribute could not be read": ["属性无法读取", "Attribute could not be read"],
            "attribute is absent or empty": ["属性缺失或为空", "Attribute is absent or empty"],
            "malformed numeric value": ["数值格式无效", "Malformed numeric value"],
            "malformed byte count": ["字节数格式无效", "Malformed byte count"],
            "sector count overflows byte capacity": ["扇区数超过容量上限", "Sector count overflows byte capacity"],
            "NVMe hwmon interface is not present": ["NVMe hwmon 接口不存在", "NVMe hwmon interface is not present"],
            "No unprivileged kernel interface exposes NVMe SMART health; privileged ioctls are not used": ["没有非特权内核接口提供 NVMe SMART 健康信息；不使用特权 ioctl", "No unprivileged kernel interface exposes NVMe SMART health; privileged ioctls are not used"],
            "GPU hwmon interface is not present": ["GPU hwmon 接口不存在", "GPU hwmon interface is not present"],
            "kernel uevent does not expose PCI_SLOT_NAME": ["内核 uevent 未提供 PCI_SLOT_NAME", "Kernel uevent does not expose PCI_SLOT_NAME"],
            "phy80211 link is not exposed": ["未提供 phy80211 链接", "phy80211 link is not exposed"],
            "wireless statistics are not exposed by /proc/net/wireless": ["/proc/net/wireless 未提供无线统计", "Wireless statistics are not exposed by /proc/net/wireless"],
            "interface is not present in /proc/net/wireless (link may be down)": ["接口未出现在 /proc/net/wireless 中（链路可能已断开）", "Interface is not present in /proc/net/wireless (link may be down)"],
            "wireless statistics row is incomplete": ["无线统计行不完整", "Wireless statistics row is incomplete"],
            "malformed wireless statistics value": ["无线统计值格式无效", "Malformed wireless statistics value"],
            "kernel reports no measurement (sentinel value)": ["内核报告无有效测量值（哨兵值）", "Kernel reports no measurement (sentinel value)"],
            "SMART backend not implemented; no health query performed": ["SMART 后端尚未实现；未执行健康查询", "SMART backend not implemented; no health query performed"],
            "no MSI vectors exposed": ["未提供 MSI 向量", "No MSI vectors exposed"],
            "libdisplay-info could not parse EDID": ["libdisplay-info 无法解析 EDID", "libdisplay-info could not parse EDID"]
        }
        return names[value] ? t(names[value][0], names[value][1]) : String(value)
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
