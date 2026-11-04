import { activity, hardware, sensors } from "../data.js";
import { button, icon, statCard } from "../ui.js";

const deviceByCategory = (category) =>
  hardware.find((item) => item.category === category);

export function renderOverview() {
  const cpu = deviceByCategory("处理器");
  const gpu = deviceByCategory("显卡");
  const memory = deviceByCategory("内存");
  const disk = deviceByCategory("存储");
  const hotSensor = sensors.find((sensor) => sensor.id === "cpu-temp");
  return `<section class="page-heading"><div><p class="eyebrow">星期三 · 2026 年 11 月 4 日</p><h1>系统概览</h1><p class="page-description">你的设备状态一切正常。</p></div><div class="heading-actions"><span class="updated-label">${icon("refresh-cw", 14)} 刚刚更新</span>${button("生成报告", "go-reports", { icon: "file-down", className: "button button-primary" })}</div></section>
    <section class="overview-hero"><div class="hero-copy"><div class="device-label">${icon("laptop", 16)} 当前设备</div><h2>ASUS Zenbook 14 OLED</h2><p>UM3402YA · AMD Ryzen 7 7840U with Radeon 780M</p><div class="hero-tags"><span class="tag tag-green">${icon("circle-check", 13)} 运行正常</span><span class="tag">${icon("clock-3", 13)} 已运行 2 小时 14 分</span><span class="tag">${icon("terminal", 13)} Arch Linux · 6.12.8-arch1-1</span></div></div><div class="hero-summary"><div class="hero-ring"><svg viewBox="0 0 120 120" aria-label="CPU 使用率 42%"><circle cx="60" cy="60" r="48" fill="none" stroke="rgba(255,255,255,.1)" stroke-width="7"/><circle cx="60" cy="60" r="48" fill="none" stroke="#a696f5" stroke-width="7" stroke-linecap="round" stroke-dasharray="301.6" stroke-dashoffset="175" transform="rotate(-90 60 60)"/><text x="60" y="58" fill="white" text-anchor="middle" font-size="22" font-weight="650">42%</text><text x="60" y="76" fill="#b1b0c1" text-anchor="middle" font-size="9">CPU 负载</text></svg></div><div class="hero-metrics"><div><span>CPU 温度</span><strong>${hotSensor.value}°C</strong></div><div><span>内存占用</span><strong>38.7%</strong></div></div></div></section>
    <div class="section-title-row"><div><h2>关键状态</h2><span>实时传感器摘要</span></div><button class="text-button" data-action="navigate" data-page="sensors">查看全部 ${icon("arrow-right", 14)}</button></div>
    <section class="stats-grid">${statCard({ label: "处理器温度", value: "54", unit: "°C", icon: "thermometer", tone: "orange", detail: "AMD Ryzen 7 7840U", trend: "正常", chartId: "overview-temp" })}${statCard({ label: "GPU 温度", value: "49", unit: "°C", icon: "monitor-play", tone: "blue", detail: "Radeon 780M · 8% 负载", trend: "正常" })}${statCard({ label: "内存使用", value: "12.4", unit: "GB", icon: "memory-stick", tone: "green", detail: "共 32 GB · LPDDR5X", trend: "38.7%", chartId: "overview-memory" })}${statCard({ label: "磁盘健康", value: "100", unit: "%", icon: "hard-drive", tone: "violet", detail: "WD_BLACK SN850X", trend: "良好" })}</section>
    <div class="content-grid overview-content"><section class="panel chart-panel"><div class="panel-heading"><div><h2>系统负载</h2><p>过去 24 分钟 · 每 1 分钟采样</p></div><div class="legend"><span><i class="legend-dot legend-violet"></i> CPU</span><span><i class="legend-dot legend-green"></i> 内存</span></div></div><div class="main-chart"><canvas id="overview-usage" aria-label="系统负载图表"></canvas></div><div class="chart-summary"><span>当前 CPU <strong>42%</strong></span><span>当前内存 <strong>38.7%</strong></span><span>平均温度 <strong>54°C</strong></span></div></section>
    <section class="panel quick-panel"><div class="panel-heading"><div><h2>硬件速览</h2><p>主要设备与连接状态</p></div>${icon("ellipsis", 18)}</div><div class="quick-list">${[
      [cpu, "violet"],
      [gpu, "blue"],
      [memory, "green"],
      [disk, "orange"],
    ]
      .map(
        ([item, tone]) =>
          `<button class="quick-item" data-action="open-hardware" data-category="${item.category}"><span class="device-icon tone-${tone}">${icon(item.icon, 17)}</span><span class="quick-copy"><strong>${item.name}</strong><small>${item.detail}</small></span><span class="status-dot" aria-label="状态正常"></span></button>`,
      )
      .join(
        "",
      )}</div><button class="quick-footer" data-action="navigate" data-page="hardware">浏览完整硬件列表 ${icon("arrow-right", 14)}</button></section></div>
    <section class="panel activity-panel"><div class="panel-heading"><div><h2>最近活动</h2><p>诊断和测试记录</p></div>${button("查看测试记录", "navigate", { icon: "arrow-up-right" }).replace('data-action="navigate"', 'data-action="navigate" data-page="benchmarks"')}</div><div class="activity-list">${activity.map((entry) => `<div class="activity-row"><span class="activity-icon">${icon(entry.icon, 16)}</span><span class="activity-copy"><strong>${entry.title}</strong><small>${entry.detail}</small></span><span class="activity-time">${entry.time}</span></div>`).join("")}</div></section>`;
}
