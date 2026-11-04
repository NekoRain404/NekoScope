# NekoScope Native Desktop Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development with independent ownership and cross-review. The user explicitly requests concurrent implementation; this overrides the skill's serial implementer default.

**Goal:** Deliver a real-data C++20/Qt Quick desktop application preserving the H5 prototype's six workflows, then expand hardware coverage and distribution validation toward the full product specification.

**Architecture:** Linux collectors feed asynchronous QObject services. QML consumes services without reading files or launching commands. Settings, reporting and test execution have independent ownership.

**Tech Stack:** C++20, Qt 6 Core/Concurrent/Quick/QuickControls2/Test, CMake, CTest. Rust only where a concrete module justifies its integration cost.

**Spec:** `docs/superpowers/specs/2026-11-04-native-linux-architecture-design.md`

## Global Constraints

- C++20 and Qt 6 Quick/QML; real hardware data only in the native app.
- GUI runs as an ordinary user; absent optional capabilities have explicit unavailable states.
- No shell interpolation; external tests are optional, cancellable and bounded.
- Keep the H5 reference intact. No backend I/O in QML or the GUI thread.
- User explicitly authorizes concurrent subagents and Git version management. Use feature branch `feat/native-desktop` in this newly initialized repository; agents edit disjoint files and the controller stages/commits integration.
- Chinese/English and light/dark/system themes are first-class settings; preserve unknown/raw hardware field labels as diagnostics.
- JSON/HTML/CSV reports default to privacy redaction.
- Do not claim cross-distribution or hardware completeness from one machine's tests.

## File ownership and public interfaces

| Area | Owned files | Interface |
| --- | --- | --- |
| Hardware inventory | `native/InventoryCollector.*`, `native/inventory/*`, `native/tests/tst_inventory.cpp` | `neko::InventorySnapshot collectInventory()` returning `devices`, `system`; device maps contain `id/category/name/subtitle/source/status/fields`, field maps `key/value/source/status/sensitive` |
| Live sensors | `native/core/SensorService.*`, `native/sensors/*`, `native/tests/tst_sensors.cpp` | Existing `readings`, `summary`, `cpuHistory`, `temperatureHistory`, `paused`, `sample/resetStatistics/toggleFavorite`; expose async `requestSample()` for GUI scheduling |
| Frontend | `native/qml/**` only | Contexts `hardware`, `sensors`, `testRunner`, `preferences`, `reports`; root `currentPage` integer and `navigateTo(index)` for verification |
| Integration | root/native CMake, `native/app/**`, `native/tests/tst_preferences.cpp`, `native/tests/tst_reports.cpp`, `native/tests/tst_app.cpp`, `packaging/**` | HardwareService, Preferences, ReportService, app main and GUI smoke harness |
| Test engine | `native/core/TestRunner.*`, `native/tests/tst_testrunner.cpp` | Existing state/history/capabilities and start/stop interface; strengthen async lifecycle and protection |

### Task 1: Compile and deepen real hardware inventory

- [ ] Build the existing collector with Qt 6 rather than the system Qt 5 moc.
- [ ] Replace tests that reject specific real hardware names with invariant and fixture tests.
- [ ] Add timestamps/provenance and CPU cache/topology, memory/NUMA, filesystem, device names/EDID details using existing interfaces or system databases. Avoid inventing unreadable values.
- [ ] Split the 500+ line file into cohesive units where useful; retain the public collection interface.
- [ ] Verify deterministic empty/malformed fixture behavior plus a real-machine snapshot.

### Task 2: Stabilize asynchronous live sensors

- [ ] Run existing sensor tests, fix genuine defects with regression coverage.
- [ ] Add `requestSample()` performing collection off the GUI thread, with no overlapping work and safe shutdown. Keep `sample()` synchronous for deterministic tests/CLI only.
- [ ] Fix counter rates to use measured elapsed monotonic time, identify hwmon channels uniquely, clear stale summary values and bound history/statistics.
- [ ] Verify absent roots, CPU first sample, pause/thermal observation, removal, reset and interval changes without launching load.

### Task 3: Implement the polished QML frontend

- [ ] Use H5 layout, lavender accents, restrained radii, dark sidebar and generous readable content as reference.
- [ ] Implement six pages with working navigation, actual data, stateful sensor filters/favorites, stress/benchmark controls, report selection and settings.
- [ ] Add shared small visual components, GPU scene graph charts, focused keyboard states, reduced motion and responsive scrolling.
- [ ] Add English/Chinese live translations; all ordinary UI strings use translation functions. Add light/dark/system and accent choices backed by Preferences.
- [ ] Expose root `navigateTo(index)` and stable objectNames for runtime verification. No synthetic health score or fabricated charts.

### Task 4: Integrate app services, build, reports and distribution scaffolding

- [ ] Create target-based CMake with warnings and optional ASan/UBSan; register existing Qt tests through CTest.
- [ ] Implement HardwareService properties `devices/system/busy/error/lastUpdated/renderBackend`; `refresh()` collects through QtConcurrent with safe shutdown.
- [ ] Implement Preferences properties `theme/language/accent/reducedMotion`, versioned QSettings, validated setters and tests using isolated settings roots.
- [ ] Implement ReportService `preview(format, includeSensitive, categories)` and `save(url, format, includeSensitive, categories)`; expose `error/lastPath`; test HTML escaping, CSV escaping, JSON validity, redaction and category selection.
- [ ] Register contexts `hardware/sensors/testRunner/preferences/reports`, connect thermal observations and schedule async samples; add CLI `--dump-inventory`, `--smoke-test`, `--snapshot`, `--software`.
- [ ] Add desktop entry, install rules, dependency documentation and CI matrix; distinguish written CI from observed distro validation.
- [ ] Build native app and run CTest. Launch on the real display, verify the actual rendering backend and capture screenshots.

### Task 5: Harden stress/benchmark engine

- [ ] First run current command/parser/lifecycle tests and reproduce defects.
- [ ] Fix missing includes, asynchronous start, busy-state rejection, no double finish, elapsed progress, memory budget, sysbench parsing and deterministic locale.
- [ ] Terminate process groups with TERM then KILL, clean up on close; verify using harmless fixture programs rather than real stress.
- [ ] Track thermal sensor freshness separately from UI pause; avoid claiming active thermal protection when unavailable.
- [ ] Preserve invocation/versions/status in result history; failed/cancelled tests never expose success scores.

### Task 6: Cross-review and incremental coverage

- [ ] Review each owned area for spec compliance and code quality; fix critical findings before committing integrated milestones.
- [ ] Verify tests, GUI navigation/themes/languages, report exports, settings persistence, subprocess cleanup and sanitizers.
- [ ] Record verified capabilities, remaining HWINFO/AIDA64 coverage gaps, unresolved distribution checks and future module candidates in `docs/development/progress.md`.
- [ ] Continue expanding the original product goal; a first successful native build is a milestone, not full completion.
