# Native QML Frontend

## Delivered

`native/qml/` contains a Qt Quick desktop interface with a dark navigation rail, responsive scrollable workspace, lavender accent, light/dark/system themes, live Chinese/English switching, reduced-motion preference, focus-visible actions, and Ctrl+1..6 / Ctrl+, navigation. The root `Main.qml` exposes `objectName: mainWindow`, writable `currentPage`, and `navigateTo(index)` for the GUI smoke harness.

The six primary workflows are backed by the native context properties:

- Overview: current hardware/system identity, real sensor summaries and session CPU samples.
- Hardware: category/search filters, source/status per field, private-field reveal, selectable values and copy.
- Sensors: real readings and history, group/search/favorites filters, pause and statistics reset.
- Stress: bounded CPU/memory controls, optional capability state, elapsed progress, output and global stop.
- Benchmarks: optional sysbench capability, single/multi/memory modes, run status and recorded results. Baseline percentage comparisons require matching type, engine, duration, thread count and a nonempty matching `toolVersion` or `version`; missing versions show an explanatory state.
- Reports: selected inventory categories, HTML/JSON/CSV preview, privacy switch and save file dialog.
- Preferences: theme, language, accent, reduced motion and runtime capability details.

The chart uses Qt Quick Shapes / scene graph geometry over values emitted by SensorService. It shows an explicit waiting state until two real samples are available; no synthetic history or health score is used. Missing numeric values display an em dash and unavailable status.

## Runtime Contract

The pages consume the agreed context properties `hardware`, `sensors`, `testRunner`, `preferences`, and `reports`. QML uses the provided properties and invokables, including `hardware.refresh`, `sensors.resetStatistics`, `sensors.toggleFavorite`, `testRunner.startStress/startBenchmark/stop`, and `reports.preview/save`.

Inventory categories retain collector strings (`CPU`, `内存`, `主板/BIOS`, `存储`, `显卡`, `网络`, `显示器`, `音频`, `电池`, `电源`). The reports service receives those raw category strings. An empty selection disables export in the UI; `[]` is not passed as an implicit request for all categories.

The runner does not currently expose a thermal-protection availability property. The stress page therefore explains that protection requires a current readable CPU temperature and does not claim that the active run is protected. Render backend appears in the global footer and settings page after HardwareService publishes it.

## Verification

`qmllint native/qml/*.qml` completes with no QML syntax errors or managed-layout geometry warnings. It reports expected static-analysis `unqualified` warnings for Qt context properties and bindings into nested delegates, whose types are installed by the C++ application at runtime. The final command `/usr/lib/qt6/bin/qmllint --unqualified disable native/qml/*.qml` exits 0 with no output.

An isolated GUI-enabled CMake build (`/tmp/nekoscope-qml-build`) configured successfully and compiled the generated C++ for every QML source. The integrated executable build remains blocked by a `QCommandLineOption` initializer ambiguity in `native/app/main.cpp:70`, outside the frontend-owned files. Each single-name option needs an unambiguous string initializer, for example `{"dump-inventory", "…"}` instead of `{{"dump-inventory"}, "…"}`.

For frontend verification, a temporary copy of the application entry point corrected only those option initializers and was linked with the CMake-built objects into `/tmp/nekoscope-frontend-verified`. The repository entry point was not changed. This executable passed the existing smoke harness with both offscreen software rendering and the real Wayland display: seven pages × two themes × two languages, exit 0 and zero QML warnings. The latest sources, including collection/reading timestamps, were included in this verification.

Real-display screenshots were captured at `/tmp/nekoscope-qml-overview.png` and `/tmp/nekoscope-qml-verified.png`. The window reports OpenGL (RHI), renders actual inventory and sensor values, and adapts its overview cards to the desktop's available logical width. The final smoke screenshot shows Chinese dark mode and real CPU history. These are temporary verification artifacts, not packaged application assets.

The smoke harness covers component loading, navigation and theme/language changes. It does not launch stress or benchmark workloads, exercise file-dialog interaction, or verify an actual report save. No load-generating test was started during frontend verification.
