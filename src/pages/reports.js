import { hardware, sensors } from "../data.js";
import { button, icon, escapeHtml } from "../ui.js";

export function makeReport(state) {
  return {
    application: "NekoScope",
    version: "0.1.0",
    simulated: true,
    notice: "本报告包含模拟数据，不代表当前设备的真实状态。",
    generatedAt: new Date().toISOString(),
    device: {
      name: "ASUS Zenbook 14 OLED",
      system: "Arch Linux",
      ...(state.includeSensitive
        ? { serial: "DEMO-SERIAL-7840U", mac: "02:00:00:00:00:01" }
        : {}),
    },
    ...(state.reportSections.has("hardware")
      ? {
          hardware: hardware.map(({ category, name, rows }) => ({
            category,
            name,
            properties: Object.fromEntries(rows),
          })),
        }
      : {}),
    ...(state.reportSections.has("sensors")
      ? {
          sensors: sensors.map((sensor) => ({
            name: sensor.name,
            current: sensor.value,
            unit: sensor.unit,
            ...state.sensorStats[sensor.id],
          })),
        }
      : {}),
    ...(state.reportSections.has("benchmarks")
      ? {
          benchmarks: state.benchmarks.map(({ type, score, date }) => ({
            type,
            score,
            date,
            simulated: true,
          })),
        }
      : {}),
  };
}

export function reportHtml(state, standalone = false) {
  const report = makeReport(state);
  const content = `<div class="report-document"><div class="report-doc-top"><div><strong>NekoScope</strong><span>HARDWARE REPORT</span></div><span class="report-stamp">演示报告</span></div><h2>设备诊断报告</h2><p class="report-subtitle">ASUS Zenbook 14 OLED · ${new Date().toLocaleDateString("zh-CN")}</p><div class="report-summary"><div><span>操作系统</span><strong>Arch Linux</strong></div><div><span>处理器</span><strong>AMD Ryzen 7 7840U</strong></div><div><span>报告内容</span><strong>${state.reportSections.size} 个模块</strong></div><div><span>隐私保护</span><strong>${state.includeSensitive ? "包含模拟标识" : "已脱敏"}</strong></div></div>${state.includeSensitive ? "<p>序列号：DEMO-SERIAL-7840U · MAC：02:00:00:00:00:01</p>" : ""}${report.hardware ? `<h3>01 / 硬件清单</h3><table><thead><tr><th>类别</th><th>设备</th><th>状态</th></tr></thead><tbody>${report.hardware.map((item) => `<tr><td>${escapeHtml(item.category)}</td><td>${escapeHtml(item.name)}</td><td>正常</td></tr>`).join("")}</tbody></table>` : ""}${report.sensors ? `<h3>02 / 传感器快照</h3><table><thead><tr><th>传感器</th><th>当前值</th><th>最大值</th></tr></thead><tbody>${report.sensors.map((sensor) => `<tr><td>${escapeHtml(sensor.name)}</td><td>${sensor.current} ${sensor.unit}</td><td>${sensor.max} ${sensor.unit}</td></tr>`).join("")}</tbody></table>` : ""}${report.benchmarks ? `<h3>03 / 基准测试</h3><table><thead><tr><th>项目</th><th>模拟成绩</th><th>时间</th></tr></thead><tbody>${report.benchmarks.map((entry) => `<tr><td>${entry.type}</td><td>${entry.score.toLocaleString("zh-CN")}</td><td>${entry.date}</td></tr>`).join("")}</tbody></table>` : ""}<p class="report-disclaimer">${report.notice}</p><div class="report-document-footer"><span>由 NekoScope 生成</span><span>© ${new Date().getFullYear()} NekoScope</span></div></div>`;
  return standalone
    ? `<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>NekoScope · 模拟设备报告</title><style>body{font:14px/1.7 system-ui,sans-serif;color:#272936;max-width:900px;margin:50px auto;padding:24px}h2{font-size:28px}table{width:100%;border-collapse:collapse}td,th{text-align:left;border-bottom:1px solid #e8e8ee;padding:10px}th{color:#77798a}h3{margin-top:30px}.report-doc-top,.report-document-footer{display:flex;justify-content:space-between;align-items:center}.report-doc-top strong{font-size:23px}.report-doc-top span,.report-summary span{display:block;color:#77798a;font-size:11px}.report-summary{display:grid;grid-template-columns:1fr 1fr;gap:18px;background:#f7f6fb;padding:20px;border-radius:10px}.report-summary strong{display:block}.report-stamp{padding:5px 12px;border:1px solid #d8d2ef;border-radius:5px}.report-disclaimer{padding:14px;background:#f7f6fb;color:#747184}.report-document-footer{margin-top:30px;color:#888;font-size:12px}</style></head><body>${content}</body></html>`
    : content;
}

export function renderReports(state) {
  return `<section class="page-heading"><div><p class="eyebrow">数据管理</p><h1>报告中心</h1><p class="page-description">把设备信息与测试结果，整理成一份清晰的报告。</p></div><span class="tag">${icon("shield-check", 14)} 默认隐藏设备标识</span></section><div class="report-layout"><section class="panel report-config"><div class="panel-heading"><div><h2>报告设置</h2><p>选择需要包含的内容</p></div>${icon("sliders-horizontal", 17)}</div><div class="config-body"><label class="form-label">报告内容</label><div class="report-checkboxes">${[
    ["hardware", "硬件清单", "处理器、内存、存储等设备", "cpu"],
    ["sensors", "传感器快照", "当前值、峰值与平均值", "activity"],
    ["benchmarks", "基准测试记录", "成绩、参数与测试环境", "gauge"],
  ]
    .map(
      ([key, label, detail, iconName]) =>
        `<label class="report-option"><span class="device-icon tone-violet">${icon(iconName, 17)}</span><span><strong>${label}</strong><small>${detail}</small></span><input type="checkbox" data-report-section="${key}" ${state.reportSections.has(key) ? "checked" : ""}></label>`,
    )
    .join(
      "",
    )}</div><div class="privacy-block"><span>${icon("shield-check", 17)}<strong>隐私设置</strong></span><label class="switch-label"><span>包含设备标识<small>模拟序列号、MAC 地址</small></span><input id="include-sensitive" type="checkbox" role="switch" ${state.includeSensitive ? "checked" : ""}><span class="switch-ui"></span></label></div><label class="form-label">导出格式</label><div class="export-options">${button("HTML 报告", "export-html", { icon: "file-code-2", className: "button button-primary", disabled: !state.reportSections.size })}${button("JSON 数据", "export-json", { icon: "braces", disabled: !state.reportSections.size })}</div><p class="form-hint">文件将保存到浏览器的下载目录。所有导出均包含模拟数据标识。</p></div></section><section class="report-preview"><div class="preview-toolbar"><span>${icon("scan-eye", 16)} 报告预览</span><span>${state.reportSections.size} 个模块 · ${state.includeSensitive ? "包含模拟标识" : "已脱敏"}</span></div>${state.reportSections.size ? reportHtml(state) : `<div class="panel empty-state">${icon("file-search", 28)}<strong>选择报告内容</strong><span>至少选择一个模块以预览和导出。</span></div>`}</section></div>`;
}
