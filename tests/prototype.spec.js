import { test, expect } from "@playwright/test";

test("navigation and global search lead to the expected hardware", async ({
  page,
}) => {
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  await page.goto("/");
  await expect(page.getByRole("heading", { name: "系统概览" })).toBeVisible();
  await page.getByRole("button", { name: "浏览完整硬件列表" }).click();
  await expect(page).toHaveURL(/#hardware$/);
  await page.locator(".global-search-button").click();
  await page.getByLabel("全局搜索").fill("Radeon");
  await page.getByRole("button", { name: /AMD Radeon 780M/ }).click();
  await expect(
    page.getByRole("heading", { name: "AMD Radeon 780M" }),
  ).toBeVisible();
  expect(errors).toEqual([]);
});

test("sensor filtering, favorites and pause controls work", async ({
  page,
}) => {
  await page.goto("/#sensors");
  await expect(page.locator("#visible-sensor-count")).toHaveText("8");
  await page.getByLabel("搜索传感器").fill("CPU");
  await expect(page.locator("#visible-sensor-count")).toHaveText("3");
  await page.getByLabel("搜索传感器").fill("");
  await page.locator('[data-action="toggle-favorites-filter"]').click();
  await expect(page.locator("#visible-sensor-count")).toHaveText("3");
  await page.getByRole("button", { name: "暂停显示" }).click();
  await expect(page.getByRole("button", { name: "继续采集" })).toBeVisible();
  await page.getByRole("button", { name: "重置统计" }).click();
  await expect(
    page.getByRole("heading", { name: "传感器", exact: true }),
  ).toBeVisible();
});

test("stress and benchmark demos can be started and stopped safely", async ({
  page,
}) => {
  await page.goto("/#stress");
  await page.getByRole("button", { name: "CPU", exact: true }).click();
  await page.getByRole("button", { name: "开始压力测试" }).click();
  await expect(
    page.getByRole("heading", { name: "测试正在进行中" }),
  ).toBeVisible();
  await page.getByRole("button", { name: "停止测试" }).click();
  await expect(page.getByRole("heading", { name: "测试已停止" })).toBeVisible();
  await page.goto("/#benchmarks");
  await page
    .getByRole("button", { name: /内存带宽/ })
    .first()
    .click();
  await page.getByRole("button", { name: "开始基准测试" }).click();
  await expect(page.getByRole("button", { name: "取消测试" })).toBeVisible();
  await page.getByRole("button", { name: "取消测试" }).click();
  await expect(page.getByText("已取消测试，未生成成绩")).toBeVisible();
});

test("report export defaults to redacted mock data", async ({ page }) => {
  await page.goto("/#reports");
  await expect(page.locator(".report-disclaimer")).toContainText("模拟数据");
  const downloadEvent = page.waitForEvent("download");
  await page.getByRole("button", { name: "JSON 数据" }).click();
  const download = await downloadEvent;
  expect(download.suggestedFilename()).toMatch(/NekoScope-demo-.*\.json$/);
  const path = await download.path();
  const fs = await import("node:fs/promises");
  const report = JSON.parse(await fs.readFile(path, "utf8"));
  expect(report.simulated).toBe(true);
  expect(report.device.serial).toBeUndefined();
  expect(report.device.mac).toBeUndefined();
});

test("theme toggles and mobile layout stays within the viewport", async ({
  page,
}) => {
  await page.goto("/");
  if (page.viewportSize().width < 760)
    await page.getByRole("button", { name: "展开导航" }).click();
  await page.getByRole("button", { name: "深色模式" }).click();
  await expect(page.locator("html")).toHaveAttribute("data-theme", "dark");
  if (page.viewportSize().width < 760) await page.keyboard.press("Escape");
  const size = await page.evaluate(() => ({
    width: document.documentElement.clientWidth,
    scroll: document.documentElement.scrollWidth,
  }));
  expect(size.scroll).toBeLessThanOrEqual(size.width + 1);
  await page.goto("/#reports");
  await expect(page.getByRole("heading", { name: "报告中心" })).toBeVisible();
});

test("completed demos retain results across navigation and allow comparison", async ({
  page,
}) => {
  await page.clock.install();
  await page.goto("/#stress");
  await page.getByRole("button", { name: "开始压力测试" }).click();
  await page.locator(".global-search-button").click();
  await page.getByLabel("全局搜索").fill("概览");
  await page.locator(".search-result").first().click();
  await expect(page.getByRole("heading", { name: "系统概览" })).toBeVisible();
  await page.clock.fastForward(11000);
  await page.locator(".global-search-button").click();
  await page.getByLabel("全局搜索").fill("压力测试");
  await page.locator(".search-result").first().click();
  await expect(page.getByRole("heading", { name: "测试已完成" })).toBeVisible();
  await page.locator(".global-search-button").click();
  await page.getByLabel("全局搜索").fill("基准");
  await page.locator(".search-result").first().click();
  await page.getByRole("button", { name: "开始基准测试" }).click();
  await page.clock.fastForward(9000);
  await expect(page.locator(".benchmark-table tbody tr")).toHaveCount(4);
  await expect(page.locator(".result-comparison")).toBeVisible();
});
