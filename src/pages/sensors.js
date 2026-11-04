import { sensors } from "../data.js";
import { button, escapeHtml, icon, statCard } from "../ui.js";

function sensorRow(sensor, state) {
  const stats = state.sensorStats[sensor.id];
  return `<tr data-sensor-row data-id="${sensor.id}" data-group="${sensor.group}" data-name="${sensor.name.toLowerCase()}"><td><button class="star-button ${state.favorites.has(sensor.id) ? "is-favorite" : ""}" data-action="toggle-favorite" data-id="${sensor.id}" title="${state.favorites.has(sensor.id) ? "取消收藏" : "收藏传感器"}" aria-label="${state.favorites.has(sensor.id) ? "取消收藏" : "收藏传感器"}">${icon("star", 15)}</button></td><td><span class="sensor-name">${sensor.name}</span><span class="sensor-group-label">${sensor.group}</span></td><td class="sensor-current">${formatValue(sensor.value)} <small>${sensor.unit}</small></td><td>${formatValue(stats.min)} <small>${sensor.unit}</small></td><td>${formatValue(stats.max)} <small>${sensor.unit}</small></td><td>${formatValue(stats.avg)} <small>${sensor.unit}</small></td><td><span class="sensor-sparkline ${sensor.tone}"><svg viewBox="0 0 110 28" preserveAspectRatio="none"><path d="${sparkline(sensor)}"/></svg></span></td><td><span class="sensor-source">演示传感器</span></td></tr>`;
}

function formatValue(value) {
  return Number.isInteger(value)
    ? value.toLocaleString("zh-CN")
    : value.toFixed(1);
}
function sparkline(sensor) {
  const seed = sensor.value % 7;
  const points = Array.from({ length: 18 }, (_, i) => [
    i * 6.4,
    21 - (Math.sin(i * 1.3 + seed) + 1) * 8 - (i % 5 === 0 ? 3 : 0),
  ]);
  return points
    .map(([x, y], i) => `${i ? "L" : "M"} ${x.toFixed(1)} ${y.toFixed(1)}`)
    .join(" ");
}

export function renderSensors(state) {
  const favoriteCount = state.favorites.size;
  return `<section class="page-heading"><div><p class="eyebrow">实时监控 · 8 个传感器</p><h1>传感器</h1><p class="page-description">监控温度、频率、功耗和系统负载。</p></div><div class="heading-actions"><span class="live-indicator ${state.sensorPaused ? "paused" : ""}"><i></i>${state.sensorPaused ? "已暂停" : "正在更新"}</span>${button(state.sensorPaused ? "继续采集" : "暂停显示", "toggle-sensors", { icon: state.sensorPaused ? "play" : "pause" })}${button("重置统计", "reset-stats", { icon: "rotate-ccw" })}</div></section>
    <section class="sensor-summary-grid">${statCard({ label: "CPU 封装温度", value: "54.2", unit: "°C", icon: "thermometer", tone: "orange", detail: "平均 55.8 °C", trend: "峰值 72.6°" })}${statCard({ label: "CPU 封装功耗", value: "12.8", unit: "W", icon: "zap", tone: "violet", detail: "平均 13.6 W", trend: "峰值 28.4 W" })}${statCard({ label: "CPU 平均频率", value: "3.28", unit: "GHz", icon: "gauge", tone: "blue", detail: "8 核心 · 16 线程", trend: "峰值 4.89 GHz" })}${statCard({ label: "内存使用量", value: "12.4", unit: "GB", icon: "memory-stick", tone: "green", detail: "总量 32 GB", trend: "38.7%" })}</section>
    <section class="panel sensor-chart-panel"><div class="panel-heading"><div><h2>温度与负载趋势</h2><p>过去 24 分钟 · 数据间隔 1 分钟</p></div><div class="legend"><span><i class="legend-dot legend-orange"></i> CPU 温度</span><span><i class="legend-dot legend-violet"></i> CPU 负载</span></div></div><div class="sensor-chart"><canvas id="sensor-temp-large" aria-label="传感器趋势"></canvas></div></section>
    <section class="panel sensor-table-panel"><div class="panel-heading sensor-table-heading"><div><h2>全部传感器 <span class="count-badge" id="sensor-count">8</span></h2><p>当前值、最小值、最大值与平均值</p></div><div class="table-controls"><label class="search-field compact-search">${icon("search", 14)}<input type="search" placeholder="搜索传感器..." id="sensor-search" value="${escapeHtml(state.sensorQuery)}" aria-label="搜索传感器"></label><select id="sensor-group" aria-label="传感器分组"><option>全部</option>${["CPU", "GPU", "内存", "存储", "主板"].map((group) => `<option ${state.sensorGroup === group ? "selected" : ""}>${group}</option>`).join("")}</select>${button("收藏", "toggle-favorites-filter", { icon: "star", className: `button button-quiet ${state.sensorFavoritesOnly ? "active-filter" : ""}`, title: `显示收藏的 ${favoriteCount} 个传感器` })}</div></div><div class="data-table-wrap"><table class="data-table sensor-table"><thead><tr><th></th><th>传感器</th><th>当前值</th><th>最小值</th><th>最大值</th><th>平均值</th><th>趋势</th><th>来源</th></tr></thead><tbody id="sensor-rows">${sensors.map((sensor) => sensorRow(sensor, state)).join("")}</tbody></table><div class="empty-state hidden" id="sensor-empty">${icon("search-x", 21)}<strong>没有找到传感器</strong><span>更换搜索内容或筛选条件。</span></div></div><div class="table-footer"><span>显示 <strong id="visible-sensor-count">8</strong> / 8 个传感器</span><span>${icon("database", 13)} 所有数值均为模拟数据</span></div></section>`;
}
