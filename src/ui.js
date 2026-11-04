export const escapeHtml = (value) =>
  String(value).replace(
    /[&<>"']/g,
    (character) =>
      ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[
        character
      ],
  );
export const icon = (name, size = 17) =>
  `<i data-lucide="${name}" style="width:${size}px;height:${size}px"></i>`;
export const button = (label, action, options = {}) =>
  `<button class="${options.className || "button button-quiet"}" data-action="${action}" ${options.title ? `title="${escapeHtml(options.title)}" aria-label="${escapeHtml(options.title)}"` : ""} ${options.disabled ? "disabled" : ""}>${options.icon ? icon(options.icon) : ""}${label ? `<span>${escapeHtml(label)}</span>` : ""}</button>`;
export const select = (id, value, values) =>
  `<select id="${id}" aria-label="参数排序方式" data-action="set-select" data-key="${id}">${values.map((option) => `<option value="${escapeHtml(option)}" ${option === value ? "selected" : ""}>${escapeHtml(option)}</option>`).join("")}</select>`;
export const format = (value) => new Intl.NumberFormat("zh-CN").format(value);

export function statCard({
  label,
  value,
  unit = "",
  icon: iconName,
  tone = "violet",
  detail,
  trend,
  chartId,
}) {
  return `<article class="stat-card"><div class="stat-top"><span>${escapeHtml(label)}</span><span class="stat-icon tone-${tone}">${icon(iconName, 17)}</span></div><div class="stat-value">${escapeHtml(value)}<small>${escapeHtml(unit)}</small></div><div class="stat-footer"><span class="stat-detail">${detail}</span><span class="trend">${icon("arrow-up-right", 13)} ${trend}</span></div>${chartId ? `<div class="mini-chart"><canvas id="${chartId}" aria-label="${escapeHtml(label)}趋势图"></canvas></div>` : ""}</article>`;
}

export function drawCharts(state, Chart) {
  state.chartRefs.forEach((chart) => chart.destroy());
  state.chartRefs = [];
  const palette =
    state.theme === "dark"
      ? { grid: "rgba(255,255,255,.08)", label: "#9294a3" }
      : { grid: "rgba(26,30,43,.07)", label: "#8e909d" };
  const line = (id, values, color, fill = false, options = {}) => {
    const canvas = document.getElementById(id);
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    const gradient = fill
      ? ctx.createLinearGradient(0, 0, 0, canvas.height || 170)
      : color;
    if (fill) {
      gradient.addColorStop(0, `${color}2b`);
      gradient.addColorStop(1, `${color}00`);
    }
    state.chartRefs.push(
      new Chart(ctx, {
        type: options.type || "line",
        data: {
          labels: state.ticks.map(
            (_, index) =>
              options.labels?.[index] ??
              new Date(Date.now() - (23 - index) * 60000).toLocaleTimeString(
                "zh-CN",
                { hour: "2-digit", minute: "2-digit" },
              ),
          ),
          datasets: [
            {
              data: values,
              borderColor: color,
              backgroundColor: gradient,
              borderWidth: options.borderWidth || 2,
              pointRadius: 0,
              pointHoverRadius: 4,
              fill,
              tension: options.tension ?? 0.38,
              ...(options.dataset || {}),
            },
            ...(options.extraDatasets || []),
          ],
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          animation: false,
          plugins: {
            legend: { display: false },
            tooltip: {
              mode: "index",
              intersect: false,
              backgroundColor: state.theme === "dark" ? "#242635" : "#20222b",
              padding: 9,
              displayColors: false,
              callbacks: {
                label: (item) => `${item.parsed.y}${options.unit || ""}`,
              },
            },
          },
          scales: options.doughnut
            ? {}
            : {
                x: {
                  display: options.axes !== false,
                  grid: { display: false },
                  border: { display: false },
                  ticks: {
                    color: palette.label,
                    maxTicksLimit: 6,
                    maxRotation: 0,
                    font: { size: 10 },
                  },
                },
                y: {
                  display: options.axes !== false,
                  min: options.min,
                  max: options.max,
                  grid: { color: palette.grid },
                  border: { display: false, dash: [4, 5] },
                  ticks: {
                    color: palette.label,
                    maxTicksLimit: 5,
                    font: { size: 10 },
                    callback: (tick) => `${tick}${options.unit || ""}`,
                  },
                },
              },
          interaction: { intersect: false, mode: "index" },
        },
      }),
    );
  };
  line(
    "overview-usage",
    state.overviewValues || [
      22, 25, 18, 32, 27, 45, 38, 35, 48, 42, 30, 52, 47, 58, 44, 50, 40, 33,
      38, 45, 38, 42, 37, 42,
    ],
    "#9785ee",
    true,
    {
      min: 0,
      max: 100,
      unit: "%",
      extraDatasets: [
        {
          data: [
            32, 33, 34, 33, 35, 36, 35, 37, 36, 38, 37, 39, 38, 39, 40, 39, 39,
            38, 39, 40, 39, 38, 39, 38.7,
          ],
          borderColor: "#7eaf98",
          backgroundColor: "transparent",
          pointRadius: 0,
          borderWidth: 1.6,
          tension: 0.4,
          fill: false,
        },
      ],
    },
  );
  line(
    "overview-temp",
    [
      44, 46, 45, 48, 47, 50, 54, 52, 51, 55, 53, 56, 54, 52, 56, 55, 57, 54,
      53, 56, 55, 54, 53, 54,
    ],
    "#ef9662",
    false,
    { min: 20, max: 90, unit: "°", axes: false },
  );
  line(
    "overview-memory",
    [
      10, 10.5, 11.1, 10.8, 11.7, 11.2, 11.9, 12.1, 11.6, 12.4, 12.7, 12.1,
      12.9, 12.3, 13.2, 12.9, 13.1, 12.6, 13.4, 12.8, 13.1, 12.6, 12.9, 12.4,
    ],
    "#50ad91",
    false,
    { min: 0, max: 32, unit: " GB", axes: false },
  );
  line(
    "usage-large",
    state.usageValues || [
      21, 25, 18, 31, 29, 44, 38, 34, 47, 41, 29, 51, 45, 57, 43, 49, 40, 32,
      37, 44, 38, 41, 36, 42,
    ],
    "#927feb",
    true,
    { min: 0, max: 100, unit: "%" },
  );
  line(
    "sensor-temp-large",
    state.tempValues || [
      44, 46, 45, 48, 47, 50, 54, 52, 51, 55, 53, 56, 54, 52, 56, 55, 57, 54,
      53, 56, 55, 54, 53, 54,
    ],
    "#e89562",
    true,
    { min: 25, max: 90, unit: "°C" },
  );
  line(
    "stress-temp",
    state.stressTemp || [
      45, 46, 48, 53, 60, 65, 68, 66, 69, 70, 68, 69, 71, 68, 70, 69, 68, 70,
      69, 68, 70, 69, 68, 69,
    ],
    "#e58b5b",
    true,
    { min: 20, max: 100, unit: "°C" },
  );
  line(
    "stress-clock",
    state.stressClock || [
      1.7, 2.4, 3.1, 3.9, 4.3, 4.1, 4.2, 4.1, 3.8, 4, 3.9, 4.1, 3.8, 3.9, 4,
      3.9, 4.1, 4, 3.9, 4.1, 3.9, 4, 3.9, 4,
    ],
    "#6289d7",
    false,
    { min: 0, max: 5, unit: " GHz" },
  );
  line(
    "benchmark-progress-chart",
    state.benchValues || [
      62, 67, 64, 71, 68, 76, 73, 80, 78, 82, 79, 85, 82, 89, 87, 91, 88, 94,
      91, 97, 95, 100, 98, 100,
    ],
    "#72ae98",
    true,
    { min: 0, max: 110, unit: "%" },
  );
  line(
    "sensor-small-cpu",
    [
      44, 46, 45, 48, 50, 54, 51, 55, 53, 56, 54, 57, 54, 56, 55, 53, 56, 55,
      54, 55, 53, 56, 55, 54,
    ],
    "#e89562",
    false,
    { axes: false },
  );
  line(
    "sensor-small-power",
    [
      8, 9, 12, 10, 14, 19, 16, 12, 17, 13, 11, 18, 20, 16, 12, 18, 14, 17, 13,
      15, 12, 17, 15, 13,
    ],
    "#9582ed",
    false,
    { axes: false },
  );
}
