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
        ui: page.ui
        title: page.ui.t("基准测试", "Measure what matters")
        description: page.ui.t("可复现的测试参数，来自实际运行的结果。", "Reproducible parameters. Results from actual runs.")
        eyebrow: "SYSBENCH / " + page.ui.t("性能测量", "PERFORMANCE MEASUREMENT")
    }
    NoticeBanner {
        ui: page.ui
        Layout.fillWidth: true
        visible: !page.ui.capability("sysbench")
        text: page.ui.t("未找到可选工具 sysbench。安装并重新启动后可运行基准测试。", "Optional tool sysbench was not found. Install it and restart to run benchmarks.")
        tone: page.ui.warning
        surfaceColor: page.ui.warningSoft
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: page.ui.space4
        Repeater {
            model: 3
            delegate: Button {
                id: typeCard
                required property int index
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                implicitHeight: 154
                enabled: !testRunner.state.running
                hoverEnabled: true
                padding: page.ui.space5
                objectName: "benchmarkType" + index
                Accessible.name: page.labels[index]
                opacity: enabled ? 1 : 0.5
                onClicked: page.selectedType = index
                contentItem: ColumnLayout {
                    spacing: page.ui.space2
                    RowLayout {
                        Layout.fillWidth: true
                        Rectangle {
                            implicitWidth: 34
                            implicitHeight: 34
                            radius: page.ui.radiusMd
                            color: page.selectedType === typeCard.index ? page.ui.accentSoft : page.ui.soft
                            Icon {
                                anchors.centerIn: parent
                                name: typeCard.index === 2 ? "hardware" : "benchmark"
                                color: page.selectedType === typeCard.index ? page.ui.accent : page.ui.secondary
                                implicitWidth: 19
                                implicitHeight: 19
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            implicitWidth: 17
                            implicitHeight: 17
                            radius: page.ui.radiusPill
                            color: page.selectedType === typeCard.index ? page.ui.accent : "transparent"
                            border.width: page.selectedType === typeCard.index ? 0 : page.ui.borderWidth
                            border.color: page.ui.borderStrong
                            Rectangle {
                                anchors.centerIn: parent
                                implicitWidth: 5
                                implicitHeight: 5
                                radius: page.ui.radiusPill
                                color: page.ui.accentText
                                visible: page.selectedType === typeCard.index
                            }
                        }
                    }
                    Label {
                        text: page.labels[typeCard.index]
                        color: page.ui.text
                        font.pixelSize: page.ui.fontSubtitle
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: page.descriptions[typeCard.index]
                        color: page.ui.secondary
                        font.pixelSize: page.ui.fontCaption
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
                background: Rectangle {
                    radius: page.ui.radiusLg
                    color: typeCard.down ? page.ui.soft
                         : page.selectedType === typeCard.index ? page.ui.accentSoft
                         : typeCard.hovered ? page.ui.soft : page.ui.panel
                    border.width: typeCard.activeFocus ? page.ui.focusWidth
                                : page.selectedType === typeCard.index ? 1.5 : page.ui.borderWidth
                    border.color: typeCard.activeFocus || page.selectedType === typeCard.index ? page.ui.accent : page.ui.border
                    Behavior on color { ColorAnimation { duration: page.ui.durationFast } }
                    Behavior on border.color { ColorAnimation { duration: page.ui.durationFast } }
                }
            }
        }
    }
    Panel {
        ui: page.ui
        Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            spacing: page.ui.space4
            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.ui.space1
                Label {
                    text: page.labels[page.selectedType]
                    color: page.ui.text
                    font.pixelSize: page.ui.fontSubtitle
                    font.weight: Font.DemiBold
                }
                Label {
                    text: page.ui.t("结果同时记录命令、工具版本和运行状态。", "Results retain invocation, tool version and run status.")
                    color: page.ui.muted
                    font.pixelSize: page.ui.fontCaption
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            Label {
                text: page.ui.t("时长 (秒)", "Duration (s)")
                color: page.ui.secondary
                font.pixelSize: page.ui.fontSmall
                font.weight: Font.Medium
            }
            NumberField {
                id: duration
                ui: page.ui
                from: 1
                to: 300
                value: 10
                editable: true
                enabled: !testRunner.state.running
                Layout.preferredWidth: 125
                objectName: "benchmarkDurationInput"
            }
            ActionButton {
                ui: page.ui
                text: page.ui.t("开始测试", "Run benchmark")
                primary: true
                symbol: "play"
                enabled: !testRunner.state.running && page.ui.capability("sysbench")
                onClicked: testRunner.startBenchmark(page.types[page.selectedType], duration.value)
                objectName: "startBenchmarkButton"
            }
        }
    }
    TestStatus { ui: page.ui; kind: "benchmark"; Layout.fillWidth: true }
    Panel {
        ui: page.ui
        Layout.fillWidth: true
        spacing: page.ui.space3
        SectionHeader {
            ui: page.ui
            title: page.ui.t("结果历史", "Results history")
            detail: page.ui.t("仅比较相同类型、版本和参数的结果", "Compare results with matching type, version and parameters")
        }
        EmptyState {
            Layout.fillWidth: true
            Layout.topMargin: page.ui.space3
            Layout.bottomMargin: page.ui.space3
            visible: !page.results.length
            ui: page.ui
            icon: "benchmark"
            title: page.ui.t("尚无运行记录", "No runs yet")
            message: page.ui.t("完成一次测试后，结果会显示在这里。", "Your results will appear here after a test.")
        }
        Repeater {
            model: page.results
            delegate: Rectangle {
                id: resultCard
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: resultBody.implicitHeight + page.ui.space3 * 2
                radius: page.ui.radiusMd
                color: page.ui.soft
                border.width: page.ui.borderWidth
                border.color: page.ui.borderSubtle
                ColumnLayout {
                    id: resultBody
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: page.ui.space3 }
                    spacing: page.ui.space2
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: page.ui.space3
                        Label {
                            text: page.labels[Math.max(0, page.types.indexOf(resultCard.modelData.type))]
                            color: page.ui.text
                            font.pixelSize: page.ui.fontBody
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }
                        StatusChip {
                            ui: page.ui
                            text: page.ui.status(resultCard.modelData.status)
                            tone: resultCard.modelData.status === "completed" ? page.ui.green : page.ui.warning
                        }
                        Label {
                            text: resultCard.modelData.status === "completed" && resultCard.modelData.score !== undefined
                                  ? page.ui.metric(resultCard.modelData.score, 2) + " " + resultCard.modelData.unit : "—"
                            color: resultCard.modelData.status === "completed" ? page.ui.accent : page.ui.muted
                            font.pixelSize: page.ui.fontTitle
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 160
                        }
                    }
                    Label {
                        text: (resultCard.modelData.engine || "sysbench") + " · " + (resultCard.modelData.duration || 0) + " s · " + (resultCard.modelData.threads || "—") + page.ui.t(" 线程", " threads")
                        color: page.ui.muted
                        font.pixelSize: page.ui.fontMicro
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Label {
                        visible: !!(resultCard.modelData.reasonKey || resultCard.modelData.reason)
                        text: page.ui.trKey(resultCard.modelData.reasonKey || "", resultCard.modelData.reasonParams, resultCard.modelData.reason || "")
                        color: resultCard.modelData.status === "failed" ? page.ui.warning : page.ui.muted
                        font.pixelSize: page.ui.fontMicro
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        visible: resultCard.modelData.status === "completed" && resultCard.modelData.score !== undefined
                        Layout.fillWidth: true
                        spacing: page.ui.space3
                        ActionButton {
                            ui: page.ui
                            compact: true
                            text: page.baselineRecord === JSON.stringify(resultCard.modelData)
                                  ? page.ui.t("当前比较基准", "Current baseline")
                                  : page.ui.t("设为比较基准", "Use as baseline")
                            onClicked: page.baselineRecord = JSON.stringify(resultCard.modelData)
                        }
                        Label {
                            readonly property var referenceResult: page.baselineRecord ? JSON.parse(page.baselineRecord) : ({})
                            readonly property string referenceVersion: referenceResult.toolVersion || referenceResult.version || ""
                            readonly property string resultVersion: resultCard.modelData.toolVersion || resultCard.modelData.version || ""
                            readonly property bool comparable: referenceVersion !== "" && referenceVersion === resultVersion && referenceResult.type === resultCard.modelData.type && referenceResult.engine === resultCard.modelData.engine && referenceResult.duration === resultCard.modelData.duration && referenceResult.threads === resultCard.modelData.threads && referenceResult.score > 0
                            visible: page.baselineRecord !== "" && page.baselineRecord !== JSON.stringify(resultCard.modelData)
                            text: comparable ? page.ui.t("相对基准 ", "Versus baseline ") + ((resultCard.modelData.score / referenceResult.score - 1) >= 0 ? "+" : "") + ((resultCard.modelData.score / referenceResult.score - 1) * 100).toFixed(1) + "%" : page.ui.t("参数不同或缺少版本，不作直接比较", "Different parameters or missing version; no direct comparison")
                            color: comparable ? page.ui.accent : page.ui.muted
                            font.pixelSize: page.ui.fontCaption
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                    CheckField {
                        id: showDetails
                        ui: page.ui
                        text: page.ui.t("查看运行上下文与原始记录", "Show context and raw record")
                    }
                    Rectangle {
                        visible: showDetails.checked
                        Layout.fillWidth: true
                        implicitHeight: detailsText.implicitHeight + page.ui.space3
                        radius: page.ui.radiusSm
                        color: page.ui.panel
                        border.width: page.ui.borderWidth
                        border.color: page.ui.borderSubtle
                        TextEdit {
                            id: detailsText
                            anchors { left: parent.left; right: parent.right; top: parent.top; margins: page.ui.space2 }
                            text: JSON.stringify(resultCard.modelData, null, 2)
                            readOnly: true
                            selectByMouse: true
                            color: page.ui.secondary
                            font.pixelSize: page.ui.fontMicro
                            font.family: page.ui.monoFamily
                            wrapMode: TextEdit.WrapAnywhere
                        }
                    }
                }
            }
        }
    }
}
