# Development progress

Date: 2026-11-04
Phase 2 status: implementation, task reviews and final whole-branch review complete
Baseline commit: `7ff0b23` (native Qt Quick foundation)
Integration commits: `fef5a71` (history + Markdown), `34be2d4` (inventory depth),
`2188fa6` (QML polish), `02ca1f6` (docs/CI/packaging harden), `6efcf69` (final-review fixes)
Branch: `feat/native-desktop`

## Verified on this host

Host: Arch Linux rolling, GCC 16.2.1, Qt 6.12.0, CMake 4.4.5, Ninja 1.13.2.
These results describe this host only; they do not establish coverage on other
distributions or hardware.

| Area | Evidence |
| --- | --- |
| Native build | `cmake -S . -B build-gui -G Ninja -DNEKOSCOPE_BUILD_GUI=ON -DBUILD_TESTING=ON` succeeds |
| CTest | 5/5 suites pass: inventory, sensors, testrunner, preferences, reports |
| GUI smoke (offscreen) | `QT_QPA_PLATFORM=offscreen ./build-gui/nekoscope --smoke-test --software` exits 0, zero QML warnings |
| GUI smoke (XWayland) | `QT_QPA_PLATFORM=xcb ./build-gui/nekoscope --smoke-test --snapshot ...` exits 0; backend reported as `OpenGL (RHI)` |
| Native Wayland | Blocked by the test environment: on niri the window is visible but never exposed, and a minimal Qt Quick window reproduces it. Not observed as an application defect. |
| Hardware snapshot | `./build-gui/nekoscope --dump-inventory` exits 0 and emits valid redacted JSON |
| H5 prototype | `npm test` 12/12 Playwright tests pass; `npm run build` succeeds |
| Sanitizers | Inventory and sensor suites previously passed isolated ASan/UBSan runs (see their reports) |
| QML lint | `qmllint --unqualified disable native/qml/*.qml` exits 0 with no output |
| Snapshot | `--snapshot` waits for the page transition; offscreen and XWayland images contain the real overview |

## Capability status

Delivered and verified against real interfaces:

- CPU identity, topology, cache, frequency, vulnerabilities and flags.
- Memory, swap, NUMA, huge pages where the kernel exposes them.
- DMI/board/firmware/system identity with sensitive fields flagged.
- Block devices, partitions, mounts, filesystem use, rotational/removable state.
- PCI devices with subsystem IDs, driver, IRQ, NUMA, link speed/width.
- USB devices with topology, interfaces, speed and sensitive serial handling.
- Network interfaces; Wi-Fi link quality when the interface is up.
- GPU identity and readable driver counters (amdgpu/i915 attributes, hwmon).
- NVMe controller/namespace identity and temperature; SMART explicitly unavailable.
- Battery energy/charge/health only from matching valid values.
- ALSA audio cards and DRM displays/connectors.
- Asynchronous sensors with bounded history, pause, reset, favorites and thermal freshness.
- Stress/benchmark runs with capability discovery, bounded parameters, process-group cleanup and no fabricated scores.
- JSON/HTML/CSV/Markdown reports with redaction and category selection.
- Chinese/English live switching; light/dark/system themes; accent and reduced motion.

## Known coverage gaps

- No privileged SMART/NVMe health, SPD/DIMM identity or vendor GPU performance counters.
- Intel `xe` GPU counters are not implemented; amdgpu/i915 are the current adapters.
- USB interface classes and PCI vendor names depend on raw IDs or the installed `pci.ids`.
- Wi-Fi quality is the kernel scale, not normalized, and absent while the link is down.
- No UDisks2, libudev, hwloc, libdrm or vendor GPU API integration yet.
- Optional tool versions and result history are recorded, but cross-machine comparison is a UI summary, not a ranking.
- Cross-distribution builds are described in CI but have not been observed end-to-end.

## CI and distribution

- `.github/workflows/ci.yml` is written for Ubuntu 24.04 plus Fedora, Arch and
  openSUSE containers, plus the Playwright prototype job. It has not been run in
  a hosted CI environment yet; treat it as unverified automation.
- Packaging includes a desktop entry, AppStream metadata and a scalable icon
  install rule. No distro package recipes or AppImage yet.

## Rust boundary

No Rust is linked in this milestone. `docs/development/rust-boundary.md` records
the four adoption gates and selects a privilege-separated read-only probe
(SMART/NVMe/SPD/DMI) as the first qualified candidate for Phase C.

## Next milestones

1. Finish the Apple-grade QML polish pass and verify on the real Wayland display.
2. Add the privileged read-only probe behind Polkit, with explicit unavailable states.
3. Add UDisks2/libudev adapters and GPU vendor counters behind capability detection.
4. Validate the CI matrix and produce distro packages plus an AppImage.
5. Add benchmark history comparison and export across machines without ranking claims.

## Final review outcome (2026-11-04)

The whole-branch review of `de9c154..02ca1f6` returned "with fixes" for three
contained issues, now being closed in a final fix wave:

1. The NVMe namespace WWID was not marked sensitive and the redaction regex
   missed `wwid`; sensitive maps also did not mask `rawValue`.
2. TestRunner state/history messages and ReportService errors were authored in
   one language and rendered verbatim, bypassing the EN/ZH switch.
3. Per-field `reason` was collected but never shown on the hardware page or in
   HTML/CSV/Markdown reports.

The same wave also fixes the missing `read-error`/`sensor-lost` status tokens,
the thermal signal wiring (`temperatureSampled`), the QML hard-coded report
format list, subzero chart filtering, the Preferences schema marker, and adds a
`qmllint` CI step.

## Deliberate scope and open decisions

These are recorded so they are not mistaken for verified completeness:

- Multi-select sensor/device filters and multi-series charts are Phase C.
- The 90 °C thermal limit is fixed in code and not adjustable in the UI; low
  battery and system thermal-state stop conditions are not implemented yet.
- Field-level report selection (beyond categories) is not implemented.
- Privileged SMART/SPD/DMI-serial reads require the probe described in
  `rust-boundary.md`.
- UDisks2, libudev, hwloc and vendor GPU APIs are not integrated.
- Report generation and benchmark-history rewrites run on the GUI thread; move
  them off when either grows.
- The repository has no LICENSE or third-party notice file. The project
  maintainer must choose the license before distribution; no license was
  invented here.
- Native Wayland rendering could not be observed on this host's niri session
  (minimal Qt Quick clients are also never exposed); XWayland and offscreen
  paths are verified.

The final fix wave is commit `6efcf69`: WWID and sensitive-map payload
redaction, stable ASCII message/reason/error keys with EN/ZH rendering,
per-field reason in the hardware page and HTML/CSV/Markdown reports, thermal
signal wiring, format discovery, optional libdisplay-info, subzero chart
values, schema-version preservation and a `qmllint` CI step. Default dumps now
redact WWID, serial, MAC and hostname; the raw WWID never appears.

## Final verification (2026-11-04, HEAD 6efcf69)

- `cmake --build build-gui`: exit 0.
- `ctest --test-dir build-gui --output-on-failure`: 5/5 suites pass.
- `qmllint --unqualified disable native/qml/*.qml`: exit 0, no output.
- Offscreen GUI smoke: exit 0, zero QML warnings.
- XWayland GUI smoke: exit 0; renderer `OpenGL (RHI)`; settled snapshot contains real data.
- `--dump-inventory`: exit 0, valid JSON, 57 devices, redacted; WWID/serial/MAC/hostname masked; raw `eui.` count 0.
- H5 Playwright: 12/12 pass.
- Final whole-branch review: clean after one fix wave (`6efcf69`).

## Grouped navigation and preferences (2026-11-05)

- Sensors page now groups by physical device (CPU, GPU, NVMe, Storage, Memory,
  Battery, Network, Motherboard, System, unknown hwmon chips) and then by sensor
  category, with collapsible headers, per-group counts, expand/collapse all,
  search that temporarily expands matches, and a flat mode.
- Hardware device tree now groups devices by inventory category with collapsible
  headers and counts; the repeated per-device category label is suppressed while
  grouped.
- Every sensor reading carries `deviceId` and `device`; unknown hwmon chips keep
  a stable `hwmon:<canonical-path>` identifier and their raw chip name.
- Preferences add: sample interval (0.5/1/2/5 s), history samples
  (30/60/90/150/300), Celsius/Fahrenheit, diagnostics visibility, start page,
  and reset-to-defaults with confirmation. The live sampling timer and sensor
  history bound follow these settings immediately.
- New tests cover device identity, configurable history bounds and preference
  validation/round-trip/reset.
- Verification: CTest 5/5, qmllint exit 0, offscreen and XWayland smoke exit 0
  with zero QML warnings; real screenshots captured for sensors, hardware and
  settings.

## CPU frequency presentation (2026-11-05)

Reference: s-tui's `FreqSource` uses per-CPU current frequency with an average
row, per-core rows, a hardware maximum for scale, and marks offline cores rather
than showing zero. NekoScope now follows the same model:

- The collector reads `/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq`
  for each online CPU, with policy-level readings as a fallback. Readings use
  MHz (like s-tui) for precision; the summary and overview cards present GHz.
- New readings: `Core N frequency`, `CPU frequency` (average),
  `CPU maximum frequency`, `CPU minimum frequency`, grouped under the
  `Frequency` sensor category, which now sorts before temperature.
- The sensors page adds a CPU frequency summary card showing current, maximum
  and minimum, and displays per-core frequencies as integer MHz.
- The hardware page adds readable `Current frequency (avg)`,
  `Hardware maximum frequency` and `Hardware minimum frequency` fields with
  source and raw kHz `rawValue`, while retaining the raw sysfs attributes.
- New deterministic tests cover per-core aggregation, max/min and summary
  conversion, and the hardware inventory fields.
- `--page <0-6>` was added to the diagnostic CLI so a specific page can be
  captured with `--snapshot`.
