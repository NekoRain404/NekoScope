import { button, icon, format } from "../ui.js";

const tests = [
  {
    name: "CPU 单核",
    icon: "cpu",
    description: "单线程计算与响应表现",
    label: "轻量测试",
    tone: "violet",
  },
  {
    name: "CPU 多核",
    icon: "layers",
    description: "多线程计算与吞吐表现",
    label: "推荐",
    tone: "blue",
  },
  {
    name: "内存带宽",
    icon: "memory-stick",
    description: "内存连续读写性能",
    label: "内存测试",
    tone: "green",
  },
];

export function renderBenchmarks(state) {
  const run = state.benchmark;
  const selected =
    state.benchmarks.find((item) => item.id === state.selectedBenchmark) ||
    state.benchmarks[0];
  const lastSameType = state.benchmarks.filter(
    (item) => item.type === selected?.type && item.id !== selected?.id,
  )[0];
  return `<section class="page-heading"><div><p class="eyebrow">测试实验室</p><h1>基准测试</h1><p class="page-description">使用可复现的测试，了解设备的性能表现。</p></div>${button("导出结果", "go-reports", { icon: "file-down" })}</section><div class="info-strip demo-info">${icon("info", 16)}<span>所有成绩均为模拟数据，仅用于体验测试、记录与比较流程。</span></div><section class="benchmark-cards">${tests.map((test) => `<button class="panel benchmark-card ${run.type === test.name ? "selected" : ""}" data-action="select-benchmark" data-value="${test.name}" ${run.running ? "disabled" : ""}><div class="benchmark-card-top"><span class="device-icon tone-${test.tone}">${icon(test.icon, 21)}</span><span class="tag ${test.label === "推荐" ? "tag-violet" : ""}">${test.label}</span></div><h2>${test.name}</h2><p>${test.description}</p><div class="benchmark-card-bottom"><span>约 8 秒 · 模拟</span><span class="radio-indicator"></span></div></button>`).join("")}</section><div class="benchmark-runbar panel"><div><span class="benchmark-run-icon">${icon(run.running ? "activity" : "flask-conical", 20)}</span><span><strong>${run.running ? `正在运行 ${run.type}` : "已选择 " + run.type}</strong><small>${run.running ? `已完成 ${Math.round(run.progress)}% · 请稍候` : "测试期间可切换页面，运行状态会保留"}</small></span></div>${button(run.running ? "取消测试" : "开始基准测试", run.running ? "stop-benchmark" : "start-benchmark", { icon: run.running ? "square" : "play", className: `button ${run.running ? "button-danger" : "button-primary"}` })}${run.running ? `<div class="test-progress"><span style="width:${run.progress}%"></span></div>` : ""}</div><div class="content-grid benchmark-results"><section class="panel"><div class="panel-heading"><div><h2>测试记录</h2><p>点击记录，查看成绩与测试环境</p></div><span class="count-badge">${state.benchmarks.length} 次</span></div><div class="data-table-wrap"><table class="data-table benchmark-table"><thead><tr><th>测试项目</th><th>成绩</th><th>时间</th><th>状态</th></tr></thead><tbody>${state.benchmarks.map((entry) => `<tr class="clickable-row ${selected?.id === entry.id ? "selected-row" : ""}" data-action="select-result" data-id="${entry.id}" tabindex="0" role="button" aria-label="查看${entry.type}测试结果"><td><span class="benchmark-type">${icon(entry.type.includes("内存") ? "memory-stick" : "cpu", 15)}${entry.type}</span></td><td><strong>${format(entry.score)}</strong><small> ${entry.type === "内存带宽" ? "MB/s" : "分"}</small></td><td class="muted">${entry.date}</td><td><span class="tag tag-green">已完成</span></td></tr>`).join("")}</tbody></table></div></section><section class="panel result-detail"><div class="panel-heading"><div><h2>结果详情</h2><p>${selected?.type || "尚无记录"}</p></div>${icon("trophy", 18)}</div>${selected ? `<div class="result-score"><span>模拟成绩</span><strong>${format(selected.score)}<small>${selected.type === "内存带宽" ? "MB/s" : "分"}</small></strong>${lastSameType ? `<span class="result-comparison">${selected.score >= lastSameType.score ? "↑" : "↓"} 相比上次 ${Math.abs((selected.score / lastSameType.score - 1) * 100).toFixed(1)}%</span>` : '<span class="muted">再次运行同类测试后可比较成绩</span>'}</div><div class="result-environment"><div><span>设备</span><strong>AMD Ryzen 7 7840U</strong></div><div><span>系统</span><strong>Arch Linux</strong></div><div><span>测试引擎</span><strong>NekoScope 模拟器 v0.1</strong></div><div><span>内存</span><strong>32 GB LPDDR5X</strong></div><div><span>测试时间</span><strong>${selected.date}</strong></div></div>` : ""}</section></div>`;
}
