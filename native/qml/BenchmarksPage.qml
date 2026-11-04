pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PageView {
    id: page
    property int selectedType: 0
    property string baselineRecord: ""
    readonly property var types: ["single", "multi", "memory"]
    readonly property var labels: [ui.t("单线程 CPU", "Single-core CPU"), ui.t("多线程 CPU", "Multi-core CPU"), ui.t("内存带宽", "Memory bandwidth")]
    readonly property var descriptions: [ui.t("一个工作线程，计算质数。", "Prime computation with one worker."), ui.t("使用可用逻辑处理器。", "Uses available logical processors."), ui.t("sysbench 内存吞吐测试。", "Memory throughput with sysbench.")]
    readonly property var results: testRunner.history.filter(item => item.kind === "benchmark")
    PageHeader {
        ui: page.ui; title: page.ui.t("基准测试", "Measure what matters")
        description: page.ui.t("可复现的测试参数，来自实际运行的结果。", "Reproducible parameters. Results from actual runs.")
        eyebrow: "SYSBENCH / " + page.ui.t("性能测量", "PERFORMANCE MEASUREMENT")
    }
    Label { visible: !page.ui.capability("sysbench"); text: page.ui.t("未找到可选工具 sysbench。安装并重新启动后可运行基准测试。", "Optional tool sysbench was not found. Install it and restart to run benchmarks."); color: page.ui.warning; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    RowLayout {
        Layout.fillWidth: true; spacing: 15
        Repeater {
            model: 3
            delegate: Button {
                id: typeCard
                required property int index
                Layout.fillWidth: true; Layout.preferredWidth: 1; implicitHeight: 152
                enabled: !testRunner.state.running; hoverEnabled: true; padding: 20
                objectName: "benchmarkType" + index
                Accessible.name: page.labels[index]
                onClicked: page.selectedType = index
                contentItem: ColumnLayout {
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Icon { name: typeCard.index === 2 ? "hardware" : "benchmark"; color: page.ui.accent; implicitWidth: 22; implicitHeight: 22 }
                        Item { Layout.fillWidth: true }
                        Rectangle { implicitWidth: 14; implicitHeight: 14; radius: 7; color: page.selectedType === typeCard.index ? page.ui.accent : "transparent"; border.color: page.selectedType === typeCard.index ? page.ui.accent : page.ui.border; Rectangle { anchors.centerIn: parent; width: 4; height: 4; radius: 2; color: "white"; visible: page.selectedType === typeCard.index } }
                    }
                    Label { text: page.labels[typeCard.index]; color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Label { text: page.descriptions[typeCard.index]; color: page.ui.secondary; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
                background: Rectangle { color: page.selectedType === typeCard.index ? page.ui.accentSoft : page.ui.panel; radius: 12; border.color: page.selectedType === typeCard.index || typeCard.activeFocus ? page.ui.accent : page.ui.border; border.width: typeCard.activeFocus ? 2 : 1 }
            }
        }
    }
    Panel {
        ui: page.ui; Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true; spacing: 15
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Label { text: page.labels[page.selectedType]; color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                Label { text: page.ui.t("结果同时记录命令、工具版本和运行状态。", "Results retain invocation, tool version and run status."); color: page.ui.muted; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
            Label { text: page.ui.t("时长 (秒)", "Duration (s)"); color: page.ui.secondary; font.pixelSize: 11 }
            SpinBox { id: duration; from: 1; to: 300; value: 10; editable: true; enabled: !testRunner.state.running; Layout.preferredWidth: 125; objectName: "benchmarkDurationInput" }
            ActionButton { ui: page.ui; text: page.ui.t("开始测试", "Run benchmark"); primary: true; symbol: "play"; enabled: !testRunner.state.running && page.ui.capability("sysbench"); onClicked: testRunner.startBenchmark(page.types[page.selectedType], duration.value); objectName: "startBenchmarkButton" }
        }
    }
    TestStatus { ui: page.ui; kind: "benchmark"; Layout.fillWidth: true }
    Panel {
        ui: page.ui; Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            Label { text: page.ui.t("结果历史", "Results history"); color: page.ui.text; font.pixelSize: 14; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            Label { text: page.ui.t("仅比较相同类型、版本和参数的结果", "Compare results with matching type, version and parameters"); color: page.ui.muted; font.pixelSize: 9 }
        }
        Label { visible: !page.results.length; text: page.ui.t("尚无运行记录。完成一次测试后，结果会显示在这里。", "No runs yet. Your results will appear here after a test."); color: page.ui.muted; font.pixelSize: 12; Layout.topMargin: 16; Layout.bottomMargin: 16 }
        Repeater {
            model: page.results
            delegate: ColumnLayout {
                id: resultRow
                required property var modelData
                Layout.fillWidth: true; spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: page.labels[Math.max(0, page.types.indexOf(resultRow.modelData.type))]; color: page.ui.text; font.pixelSize: 12; Layout.fillWidth: true }
                    Label { text: page.ui.status(resultRow.modelData.status); color: resultRow.modelData.status === "completed" ? page.ui.green : page.ui.warning; font.pixelSize: 11; Layout.preferredWidth: 95 }
                    Label { text: resultRow.modelData.status === "completed" && resultRow.modelData.score !== undefined ? page.ui.metric(resultRow.modelData.score, 2) + " " + resultRow.modelData.unit : "—"; color: page.ui.accent; font.pixelSize: 14; font.weight: Font.DemiBold; Layout.preferredWidth: 160; horizontalAlignment: Text.AlignRight }
                }
                Label { text: (resultRow.modelData.engine || "sysbench") + " · " + (resultRow.modelData.duration || 0) + " s · " + (resultRow.modelData.threads || "—") + page.ui.t(" 线程", " threads"); color: page.ui.muted; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    visible: resultRow.modelData.status === "completed" && resultRow.modelData.score !== undefined
                    Layout.fillWidth: true
                    ActionButton { ui: page.ui; compact: true; text: page.baselineRecord === JSON.stringify(resultRow.modelData) ? page.ui.t("当前比较基准", "Current baseline") : page.ui.t("设为比较基准", "Use as baseline"); onClicked: page.baselineRecord = JSON.stringify(resultRow.modelData) }
                    Label {
                        readonly property var referenceResult: page.baselineRecord ? JSON.parse(page.baselineRecord) : ({})
                        readonly property string referenceVersion: referenceResult.toolVersion || referenceResult.version || ""
                        readonly property string resultVersion: resultRow.modelData.toolVersion || resultRow.modelData.version || ""
                        readonly property bool comparable: referenceVersion !== "" && referenceVersion === resultVersion && referenceResult.type === resultRow.modelData.type && referenceResult.engine === resultRow.modelData.engine && referenceResult.duration === resultRow.modelData.duration && referenceResult.threads === resultRow.modelData.threads && referenceResult.score > 0
                        visible: page.baselineRecord !== "" && page.baselineRecord !== JSON.stringify(resultRow.modelData)
                        text: comparable ? page.ui.t("相对基准 ", "Versus baseline ") + ((resultRow.modelData.score / referenceResult.score - 1) >= 0 ? "+" : "") + ((resultRow.modelData.score / referenceResult.score - 1) * 100).toFixed(1) + "%" : page.ui.t("参数不同或缺少版本，不作直接比较", "Different parameters or missing version; no direct comparison")
                        color: comparable ? page.ui.accent : page.ui.muted; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.WordWrap
                    }
                }
                TextEdit { text: JSON.stringify(resultRow.modelData, null, 2); visible: showDetails.checked; readOnly: true; selectByMouse: true; color: page.ui.secondary; font.pixelSize: 10; font.family: "monospace"; wrapMode: TextEdit.WrapAnywhere; Layout.fillWidth: true }
                CheckBox { id: showDetails; text: page.ui.t("查看运行上下文与原始记录", "Show context and raw record"); font.pixelSize: 10 }
                Rectangle { implicitHeight: 1; color: page.ui.border; Layout.fillWidth: true }
            }
        }
    }
}
