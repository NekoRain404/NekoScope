# Development progress

Date: 2026-11-04
Phase 2 status: implementation complete, review and integration in progress
Baseline commit: `7ff0b23` (native Qt Quick foundation)
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
- JSON/HTML/CSV reports with redaction and category selection; Markdown is part of the Phase 2 change set pending final verification.
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
