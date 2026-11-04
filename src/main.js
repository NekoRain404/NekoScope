import { Chart, registerables } from "chart.js";
import { createIcons, icons } from "lucide";
import { createState, hardware, navigation, sensors } from "./data.js";
import { button, drawCharts, escapeHtml, icon } from "./ui.js";
import { renderOverview } from "./pages/overview.js";
import { renderHardware } from "./pages/hardware.js";
import { renderSensors } from "./pages/sensors.js";
import { renderStress } from "./pages/stress.js";
import { renderBenchmarks } from "./pages/benchmarks.js";
import { makeReport, renderReports, reportHtml } from "./pages/reports.js";
import "./style.css";

Chart.register(...registerables);
Chart.defaults.font.family =
  'Inter, "Noto Sans SC", "Microsoft YaHei", system-ui, sans-serif';
const state = createState();
try {
  state.theme = localStorage.getItem("nekoscope-theme") || "light";
  const favorites = JSON.parse(
    localStorage.getItem("nekoscope-favorites") || "null",
  );
  if (Array.isArray(favorites))
    state.favorites = new Set(
      favorites.filter((id) => sensors.some((sensor) => sensor.id === id)),
    );
} catch {
  /* Storage can be unavailable in private browser contexts. */
}
const renders = {
  overview: renderOverview,
  hardware: renderHardware,
  sensors: renderSensors,
  stress: renderStress,
  benchmarks: renderBenchmarks,
  reports: renderReports,
};
const app = document.querySelector("#app");
const logo = `<svg viewBox="0 0 40 40" fill="none" aria-hidden="true"><path d="M9 24V10l8 6h6l8-6v14c0 9-22 9-22 0Z" stroke="currentColor" stroke-width="2.5" stroke-linejoin="round"/><path d="M15 23h1m8 0h1" stroke="currentColor" stroke-width="2.8" stroke-linecap="round"/></svg>`;
app.innerHTML = `<div class="app-layout"><button class="sidebar-backdrop" data-action="close-sidebar" aria-label="关闭导航"></button><aside class="sidebar"><a class="brand" href="#overview"><span class="brand-symbol">${logo}</span><span>NekoScope<small>看见硬件的每一面</small></span></a><div class="workspace"><span class="workspace-avatar">N</span><span><strong>Neko 的工作站</strong><small>本地设备 · 演示环境</small></span><span class="workspace-dot"></span></div><nav aria-label="主导航">${navigation.map((item, index) => `${index === 0 || navigation[index - 1].group !== item.group ? `<p class="nav-section-label">${item.group}</p>` : ""}<a href="#${item.id}" class="nav-item" data-nav="${item.id}">${icon(item.icon, 18)}<span>${item.label}</span>${item.id === "sensors" ? '<span class="nav-count">8</span>' : ""}${item.id === "stress" ? '<span class="nav-running hidden" id="nav-running"></span>' : ""}</a>`).join("")}</nav><div class="sidebar-bottom"><div class="system-health"><div><span class="health-icon">${icon("shield-check", 17)}</span><strong>系统运行良好</strong><span class="status-dot"></span></div><p>关键传感器处于正常范围</p><div class="health-meter"><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span></div><div class="health-values"><span>CPU <strong id="sidebar-temp">54°C</strong></span><span>内存 <strong>38.7%</strong></span></div></div><button class="theme-toggle" data-action="toggle-theme">${icon("moon", 16)}<span>深色模式</span><span class="theme-switch"></span></button><div class="sidebar-footer"><span class="footer-logo">${logo}</span><span>NekoScope <small>v0.1.0 Preview</small></span><a href="#reports" aria-label="打开报告中心">${icon("arrow-up-right", 14)}</a></div></div></aside><div class="main-layout"><header class="topbar"><div class="topbar-left">${button("", "toggle-sidebar", { icon: "panel-left", title: "展开导航", className: "icon-button mobile-menu" })}<span class="breadcrumb-home">${icon("house", 15)}</span><span class="breadcrumb-divider">/</span><span id="breadcrumb">系统概览</span></div><div class="topbar-right"><button class="global-search-button" data-action="open-search" aria-label="搜索设备与传感器">${icon("search", 16)}<span>搜索设备与传感器</span><kbd>Ctrl K</kbd></button><span class="demo-badge"><span></span> 交互演示</span><span class="topbar-separator"></span>${button("", "notifications", { icon: "bell", title: "通知", className: "icon-button notification-button" })}<span class="user-avatar" title="演示用户 Neko">N</span></div></header><main id="main-content" class="main-content"></main><footer class="main-footer"><span><span class="status-dot"></span> 模拟环境 · 未连接真实硬件</span><span>Built for Linux <span class="footer-heart">♡</span><span class="footer-divider">·</span> NekoScope Preview</span></footer></div></div><div id="toast" class="toast" role="status" aria-live="polite"></div><dialog class="search-dialog" id="search-dialog"><div class="search-dialog-top">${icon("search", 20)}<input id="global-search-input" placeholder="搜索页面、设备或传感器..." aria-label="全局搜索" autocomplete="off"><button class="dialog-close" data-action="close-search" aria-label="关闭搜索"><kbd>ESC</kbd></button></div><div id="search-results" class="search-results"></div><div class="search-dialog-footer">输入关键词搜索 <span>↑↓ 选择 · Enter 打开 · Esc 关闭</span></div></dialog><dialog class="info-dialog" id="info-dialog"><div id="info-dialog-content"></div><button class="button button-primary" data-action="close-info">知道了</button></dialog>`;

let toastTimer;
let searchIndex = 0;
const searchDialog = document.querySelector("#search-dialog");
const infoDialog = document.querySelector("#info-dialog");

function hydrate() {
  createIcons({ icons, attrs: { "stroke-width": 1.7, "aria-hidden": "true" } });
}
function toast(message) {
  clearTimeout(toastTimer);
  const element = document.querySelector("#toast");
  element.textContent = message;
  element.classList.add("visible");
  toastTimer = setTimeout(() => element.classList.remove("visible"), 3200);
}
function persist(key, value) {
  try {
    localStorage.setItem(key, value);
  } catch {
    /* Preferences remain available for this session. */
  }
}
function applyTheme() {
  document.documentElement.dataset.theme = state.theme;
  const toggle = document.querySelector(".theme-toggle");
  toggle.innerHTML = `${icon(state.theme === "dark" ? "sun" : "moon", 16)}<span>${state.theme === "dark" ? "浅色模式" : "深色模式"}</span><span class="theme-switch ${state.theme === "dark" ? "on" : ""}"></span>`;
  toggle.setAttribute("aria-pressed", String(state.theme === "dark"));
}
function navigate(page) {
  if (location.hash === `#${page}`) render();
  else location.hash = page;
  document.body.classList.remove("sidebar-open");
}
function render() {
  const page = location.hash.slice(1).split("?")[0] || "overview";
  state.page = renders[page] ? page : "overview";
  if (page !== state.page) history.replaceState(null, "", `#${state.page}`);
  document.querySelectorAll("[data-nav]").forEach((element) => {
    const active = element.dataset.nav === state.page;
    element.classList.toggle("active", active);
    if (active) element.setAttribute("aria-current", "page");
    else element.removeAttribute("aria-current");
  });
  const title = navigation.find((item) => item.id === state.page).label;
  document.title = `NekoScope · ${title}`;
  document.querySelector("#breadcrumb").textContent = title;
  document.querySelector("#main-content").innerHTML =
    renders[state.page](state);
  hydrate();
  drawCharts(state, Chart);
  if (state.page === "sensors") filterSensors();
}

function updateSearch(query = "") {
  const normalized = query.toLowerCase().trim();
  const results = [
    ...navigation.map((item) => ({
      title: item.label,
      description: "页面",
      icon: item.icon,
      page: item.id,
    })),
    ...hardware.map((item) => ({
      title: item.name,
      description: item.category,
      icon: item.icon,
      page: "hardware",
      category: item.category,
    })),
    ...sensors.map((item) => ({
      title: item.name,
      description: `传感器 · ${item.group}`,
      icon: item.icon,
      page: "sensors",
      query: item.name,
    })),
  ].filter((item) =>
    `${item.title} ${item.description}`.toLowerCase().includes(normalized),
  );
  document.querySelector("#search-results").innerHTML = results.length
    ? `<p class="search-label">${normalized ? `找到 ${results.length} 个结果` : "快速访问"}</p>${results
        .slice(0, 12)
        .map(
          (item, index) =>
            `<button class="search-result ${index === 0 ? "search-selected" : ""}" data-action="search-result" data-page="${item.page}" data-category="${item.category || ""}" data-query="${item.query || ""}">${icon(item.icon, 19)}<span><strong>${escapeHtml(item.title)}</strong><small>${item.description}</small></span>${icon("arrow-up-left", 14)}</button>`,
        )
        .join("")}`
    : '<div class="empty-state"><strong>没有找到匹配内容</strong><span>试试 CPU、内存、温度或报告。</span></div>';
  searchIndex = 0;
  hydrate();
}

function filterSensors() {
  const rows = document.querySelectorAll("[data-sensor-row]");
  let count = 0;
  rows.forEach((row) => {
    const visible =
      row.dataset.name.includes(state.sensorQuery.toLowerCase().trim()) &&
      (state.sensorGroup === "全部" ||
        row.dataset.group === state.sensorGroup) &&
      (!state.sensorFavoritesOnly || state.favorites.has(row.dataset.id));
    row.hidden = !visible;
    if (visible) count++;
  });
  for (const id of ["sensor-count", "visible-sensor-count"]) {
    const el = document.getElementById(id);
    if (el) el.textContent = count;
  }
  document
    .querySelector("#sensor-empty")
    ?.classList.toggle("hidden", count > 0);
}

function download(content, type, extension) {
  const url = URL.createObjectURL(new Blob([content], { type }));
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = `NekoScope-demo-${new Date().toISOString().slice(0, 10)}.${extension}`;
  document.body.append(anchor);
  anchor.click();
  anchor.remove();
  setTimeout(() => URL.revokeObjectURL(url), 2000);
  toast(`模拟报告已导出为 ${extension.toUpperCase()}`);
}

function info(title, text) {
  document.querySelector("#info-dialog-content").innerHTML =
    `<span class="info-modal-icon">${icon("info", 25)}</span><h2>${escapeHtml(title)}</h2><p>${escapeHtml(text)}</p>`;
  hydrate();
  infoDialog.showModal();
}

app.addEventListener("click", (event) => {
  if (event.target.closest("[data-nav]"))
    document.body.classList.remove("sidebar-open");
  const target = event.target.closest("[data-action]");
  if (!target || target.disabled) return;
  const action = target.dataset.action;
  switch (action) {
    case "navigate":
      navigate(target.dataset.page);
      break;
    case "go-reports":
      navigate("reports");
      break;
    case "open-hardware":
      state.activeHardware = target.dataset.category;
      navigate("hardware");
      break;
    case "toggle-theme":
      state.theme = state.theme === "light" ? "dark" : "light";
      persist("nekoscope-theme", state.theme);
      applyTheme();
      render();
      break;
    case "toggle-sidebar":
      document.body.classList.toggle("sidebar-open");
      break;
    case "close-sidebar":
      document.body.classList.remove("sidebar-open");
      break;
    case "open-search":
      searchDialog.showModal();
      document.querySelector("#global-search-input").value = "";
      updateSearch();
      document.querySelector("#global-search-input").focus();
      break;
    case "close-search":
      searchDialog.close();
      break;
    case "close-info":
      infoDialog.close();
      break;
    case "search-result":
      if (target.dataset.category)
        state.activeHardware = target.dataset.category;
      if (target.dataset.page === "sensors") {
        state.sensorQuery = target.dataset.query;
        state.sensorGroup = "全部";
        state.sensorFavoritesOnly = false;
      }
      searchDialog.close();
      navigate(target.dataset.page);
      break;
    case "notifications":
      info(
        "通知中心",
        state.stress.running || state.benchmark.running
          ? "当前有模拟测试正在运行。你可以切换到测试页面查看进度，或随时停止。"
          : "所有关键传感器处于正常范围。当前为模拟演示环境，没有真实设备告警。",
      );
      break;
    case "field-info":
      info(
        target.getAttribute("title"),
        "此字段用于展示硬件识别结果。当前内容来自模拟设备数据库，未读取本机硬件。真实版本会在此说明字段含义、采集接口及权限要求。",
      );
      break;
    case "toggle-favorite":
      state.favorites.has(target.dataset.id)
        ? state.favorites.delete(target.dataset.id)
        : state.favorites.add(target.dataset.id);
      persist("nekoscope-favorites", JSON.stringify([...state.favorites]));
      render();
      break;
    case "toggle-favorites-filter":
      state.sensorFavoritesOnly = !state.sensorFavoritesOnly;
      render();
      break;
    case "toggle-sensors":
      state.sensorPaused = !state.sensorPaused;
      render();
      toast(state.sensorPaused ? "已暂停传感器显示" : "已恢复传感器显示");
      break;
    case "reset-stats":
      sensors.forEach((sensor) => {
        state.sensorStats[sensor.id] = {
          min: sensor.value,
          max: sensor.value,
          avg: sensor.value,
          count: 1,
        };
      });
      render();
      toast("统计已重置，从当前值重新开始");
      break;
    case "set-stress-mode":
      state.stress.mode = target.dataset.value;
      render();
      break;
    case "stress-preset":
      Object.assign(
        state.stress,
        target.dataset.value === "quick"
          ? { mode: "CPU", duration: 10, load: 100 }
          : { mode: "CPU + 内存", duration: 30, load: 100 },
      );
      render();
      break;
    case "start-stress":
      if (state.benchmark.running) {
        toast("请先停止或等待基准测试完成");
        break;
      }
      Object.assign(state.stress, {
        running: true,
        completed: false,
        stopped: false,
        progress: 0,
        startedAt: Date.now(),
      });
      state.stressTemp = Array(24).fill(45);
      state.stressClock = Array(24).fill(1.5);
      render();
      toast("模拟压力测试已开始");
      break;
    case "stop-stress":
      Object.assign(state.stress, {
        running: false,
        stopped: true,
        completed: false,
      });
      render();
      toast("已停止模拟压力测试");
      break;
    case "select-benchmark":
      state.benchmark.type = target.dataset.value;
      render();
      break;
    case "start-benchmark":
      if (state.stress.running) {
        toast("请先停止或等待压力测试完成");
        break;
      }
      Object.assign(state.benchmark, {
        running: true,
        progress: 0,
        startedAt: Date.now(),
      });
      render();
      toast("模拟基准测试已开始");
      break;
    case "stop-benchmark":
      state.benchmark.running = false;
      state.benchmark.progress = 0;
      render();
      toast("已取消测试，未生成成绩");
      break;
    case "select-result":
      state.selectedBenchmark = Number(target.dataset.id);
      render();
      break;
    case "export-html":
      download(reportHtml(state, true), "text/html;charset=utf-8", "html");
      break;
    case "export-json":
      download(
        JSON.stringify(makeReport(state), null, 2),
        "application/json;charset=utf-8",
        "json",
      );
      break;
  }
});

app.addEventListener("input", (event) => {
  const target = event.target;
  if (target.id === "global-search-input") updateSearch(target.value);
  if (target.id === "sensor-search") {
    state.sensorQuery = target.value;
    filterSensors();
  }
  if (target.dataset.action === "hardware-search") {
    const rows = document.querySelectorAll("#hardware-table tbody tr");
    let visible = 0;
    rows.forEach((row) => {
      row.hidden = !row.textContent
        .toLowerCase()
        .includes(target.value.toLowerCase().trim());
      if (!row.hidden) visible++;
    });
    document
      .querySelector("#hardware-empty")
      ?.classList.toggle("hidden", visible > 0);
  }
  if (target.id === "stress-load") {
    state.stress.load = Number(target.value);
    document.querySelector("#load-value").textContent = `${target.value}%`;
  }
});
app.addEventListener("change", (event) => {
  const target = event.target;
  if (target.id === "sensor-group") {
    state.sensorGroup = target.value;
    filterSensors();
  }
  if (target.id === "stress-duration")
    state.stress.duration = Number(target.value);
  if (target.id === "stress-threads") state.stress.threads = target.value;
  if (target.id === "hardware-sort") {
    const tbody = document.querySelector("#hardware-table tbody");
    const rows = [...tbody.rows];
    if (target.value === "按名称")
      rows.sort((a, b) =>
        a.cells[0].textContent.localeCompare(b.cells[0].textContent, "zh-CN"),
      );
    else
      rows.sort(
        (a, b) =>
          hardware
            .find((item) => item.category === state.activeHardware)
            .rows.findIndex(([key]) => key === a.cells[0].textContent) -
          hardware
            .find((item) => item.category === state.activeHardware)
            .rows.findIndex(([key]) => key === b.cells[0].textContent),
      );
    rows.forEach((row) => tbody.append(row));
  }
  if (target.dataset.reportSection) {
    target.checked
      ? state.reportSections.add(target.dataset.reportSection)
      : state.reportSections.delete(target.dataset.reportSection);
    render();
  }
  if (target.id === "include-sensitive") {
    state.includeSensitive = target.checked;
    render();
  }
});

document.addEventListener("keydown", (event) => {
  if (
    ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k") ||
    (event.key === "/" &&
      !["INPUT", "TEXTAREA", "SELECT"].includes(document.activeElement.tagName))
  ) {
    event.preventDefault();
    if (!searchDialog.open) {
      searchDialog.showModal();
      updateSearch();
      document.querySelector("#global-search-input").focus();
    }
  }
  if (event.key === "Escape") document.body.classList.remove("sidebar-open");
  if (
    searchDialog.open &&
    ["ArrowDown", "ArrowUp", "Enter"].includes(event.key)
  ) {
    const results = [...document.querySelectorAll(".search-result")];
    if (!results.length) return;
    event.preventDefault();
    if (event.key === "Enter") results[searchIndex]?.click();
    else {
      searchIndex =
        (searchIndex + (event.key === "ArrowDown" ? 1 : -1) + results.length) %
        results.length;
      results.forEach((result, index) =>
        result.classList.toggle("search-selected", index === searchIndex),
      );
      results[searchIndex].scrollIntoView({ block: "nearest" });
    }
  }
  if (event.key === "Enter" && document.activeElement.matches(".clickable-row"))
    document.activeElement.click();
});
for (const dialog of [searchDialog, infoDialog])
  dialog.addEventListener("click", (event) => {
    const rect = dialog.getBoundingClientRect();
    if (
      event.target === dialog &&
      (event.clientX < rect.left ||
        event.clientX > rect.right ||
        event.clientY < rect.top ||
        event.clientY > rect.bottom)
    )
      dialog.close();
  });

setInterval(() => {
  let shouldRender = false;
  if (state.stress.running) {
    const elapsed = (Date.now() - state.stress.startedAt) / 1000;
    state.stress.progress = Math.min(
      100,
      (elapsed / state.stress.duration) * 100,
    );
    const intensity =
      (state.stress.load / 100) * (state.stress.mode === "内存" ? 0.4 : 1);
    const threadCount = state.stress.threads?.startsWith("1 ")
      ? 1
      : state.stress.threads?.startsWith("4 ")
        ? 4
        : state.stress.threads?.startsWith("8 ")
          ? 8
          : 16;
    const temp =
      45 + Math.min(37, elapsed * 5) * intensity * (0.5 + threadCount / 32);
    state.stressTemp = [...state.stressTemp.slice(1), Number(temp.toFixed(1))];
    state.stressClock = [
      ...state.stressClock.slice(1),
      Number((2.7 + 1.5 * intensity + Math.sin(elapsed) * 0.1).toFixed(2)),
    ];
    if (state.stress.progress >= 100) {
      state.stress.running = false;
      state.stress.completed = true;
      toast("模拟压力测试已完成");
    }
    shouldRender = state.page === "stress";
  }
  if (state.benchmark.running) {
    state.benchmark.progress = Math.min(
      100,
      (Date.now() - state.benchmark.startedAt) / 80,
    );
    if (state.benchmark.progress >= 100) {
      state.benchmark.running = false;
      const baseline = { "CPU 多核": 12842, "CPU 单核": 1842, 内存带宽: 68542 }[
        state.benchmark.type
      ];
      const entry = {
        id: Date.now(),
        type: state.benchmark.type,
        score: Math.round(baseline * (1.01 + Math.random() * 0.04)),
        date: new Date().toLocaleString("sv-SE").slice(0, 16),
        state: "已完成",
      };
      state.benchmarks.unshift(entry);
      state.selectedBenchmark = entry.id;
      toast("模拟基准测试完成，成绩已保存");
    }
    shouldRender ||= state.page === "benchmarks";
  }
  document
    .querySelector("#nav-running")
    .classList.toggle("hidden", !state.stress.running);
  if (shouldRender) render();
}, 500);

let telemetryTick = 0;
setInterval(() => {
  telemetryTick++;
  if (state.sensorPaused) return;
  sensors.forEach((sensor, index) => {
    const stats = state.sensorStats[sensor.id];
    const wave = Math.sin(telemetryTick * 0.38 + index) * 0.8;
    const value = Number((sensor.value + wave).toFixed(1));
    stats.min = Math.min(stats.min, value);
    stats.max = Math.max(stats.max, value);
    stats.count = (stats.count || 1) + 1;
    stats.avg += (value - stats.avg) / stats.count;
    const row = document.querySelector(
      `[data-sensor-row][data-id="${sensor.id}"]`,
    );
    if (row) {
      const print = (v) =>
        Number.isInteger(v) ? v.toLocaleString("zh-CN") : v.toFixed(1);
      [value, stats.min, stats.max, stats.avg].forEach((v, i) => {
        row.cells[i + 2].innerHTML =
          `${print(v)} <small>${sensor.unit}</small>`;
      });
    }
  });
  const temp = `${(54.2 + Math.sin(telemetryTick * 0.38) * 0.8).toFixed(0)}°C`;
  document.querySelector("#sidebar-temp").textContent = temp;
  const overviewChart = state.chartRefs.find(
    (chart) => chart.canvas.id === "overview-usage",
  );
  if (overviewChart) {
    overviewChart.data.datasets[0].data.push(
      Math.round(36 + Math.sin(telemetryTick * 0.6) * 10),
    );
    overviewChart.data.datasets[0].data.shift();
    overviewChart.update("none");
  }
}, 2000);

window.addEventListener("hashchange", () => {
  document.body.classList.remove("sidebar-open");
  render();
  window.scrollTo({ top: 0 });
});
applyTheme();
render();
