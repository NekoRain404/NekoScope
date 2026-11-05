# NekoScope

NekoScope is a Linux hardware information and diagnostics application with two
codebases in this repository:

- `native/` — the real application: C++20, Qt 6 Quick/QML, real procfs/sysfs
  collection, asynchronous sensors, optional stress/benchmark tools and reports.
- `src/`, `tests/`, `index.html` — the original H5 prototype with simulated
  data. It remains the visual/content reference and the Playwright UI-test
  fixture. It never feeds data into the native application.

## Native application

### Requirements

- CMake 3.22+, Ninja, a C++20 compiler, Qt 6.4+ (`Core`, `Concurrent`, `Gui`,
  `Qml`, `Quick`, `QuickControls2`, `Test`).
- Optional runtime tools: `stress-ng` for stress tests, `sysbench` for
  benchmarks. Missing tools appear as unavailable capabilities; nothing is
  installed automatically.
- Optional collectors: `libdisplay-info` for richer EDID parsing. It is enabled
  automatically when pkg-config finds the library; set
  `-DNEKOSCOPE_USE_DISPLAY_INFO=OFF` to disable it. Without it, the collector
  keeps the conservative EDID base-block parser.

### Build and test

```sh
cmake -S . -B build -G Ninja -DNEKOSCOPE_BUILD_GUI=ON -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the application:

```sh
./build/nekoscope
```

Diagnostics and verification:

```sh
./build/nekoscope --dump-inventory        # redacted real snapshot as JSON
QT_QPA_PLATFORM=offscreen ./build/nekoscope --smoke-test --software
./build/nekoscope --snapshot /tmp/nekoscope.png
./build/nekoscope --page 2 --snapshot /tmp/sensors.png   # capture a specific page
./build/nekoscope --software              # force software rendering
```

The footer and Settings page show the Qt Quick render backend actually in use
(OpenGL/Vulkan RHI or software). `--software` is the fallback for problematic
GPU drivers.

### Capabilities

- Overview, hardware detail, sensors, stress test, benchmarks, reports and
  preferences, with live Chinese/English switching and light/dark/system themes.
- HWiNFO-style grouped navigation: sensors by physical device then sensor type,
  and the hardware tree by inventory category, both collapsible with a flat mode.
- Preferences for sample interval, retained history length, Celsius/Fahrenheit,
  diagnostics visibility, start page and reset-to-defaults.
- Every inventory field carries its source, UTC capture time and read status;
  unknown, unavailable, permission-denied and read-error states are distinct.
- Sensor sampling runs off the GUI thread, with bounded history, favorites,
  pause and reset. Thermal protection is only claimed while a fresh CPU
  temperature is readable.
- Stress and benchmark runs are bounded, cancellable, process-group cleaned and
  never produce a success score for failed or stopped work. Benchmark history
  persists across launches.
- Reports export JSON, HTML, CSV and Markdown with category selection and
  redaction enabled by default.

### Architecture

| Path | Responsibility |
| --- | --- |
| `native/core/` | sensor service, stress/benchmark lifecycle |
| `native/inventory/` | Linux collectors split by domain (CPU/memory, devices, GPU, ...) |
| `native/sensors/` | non-QObject sysfs/hwmon collection |
| `native/app/` | Qt services, preferences, reports, entry point |
| `native/qml/` | pages and shared visual components |
| `native/tests/` | deterministic Qt Test suites with fixture roots |

Rust is not linked into the application in this milestone. The decision record
`docs/development/rust-boundary.md` defines the adoption gates and selects a
privilege-separated read-only probe as the first qualified candidate.

### Packaging

CMake installs the binary, desktop entry, AppStream metadata and scalable icon.
Linux packaging metadata lives in `packaging/linux/`. Distribution packages
should declare Qt 6 and optional tools accurately and keep the application
runnable by an ordinary user without root.

## H5 prototype

Requirements: Node.js 20.19+ or 22.12+.

```sh
npm install
npm run dev      # http://localhost:4173/
npm test         # Playwright, desktop + mobile
npm run build    # production bundle in dist/
```

All prototype data is simulated and labelled as such.

## Documentation

- Product specification: `docs/superpowers/specs/2026-11-04-native-linux-architecture-design.md`
- Implementation plan: `docs/superpowers/plans/2026-11-04-native-desktop.md`
- Development reports: `docs/development/`
