# Live sensor implementation and verification

Date: 2026-11-04. Verified on the current Linux host using Qt 6.12.0 and GCC 16.2.1; this does not establish other hardware or distribution coverage.

## Architecture and interface

`native/sensors/SensorCollector.{h,cpp}` contains plain collection state, with no QObject or UI access. Add `native/sensors/SensorCollector.cpp` to the native core target. `SensorService` requires Qt6::Core and Qt6::Concurrent; its tests additionally require Qt6::Test.

Existing readings, summary, CPU/temperature histories, pause, favorites and reset interfaces remain. GUI timers must call `requestSample()`; synchronous `sample()` is for CLI/tests. While an async sample is pending, both entry points reject additional requests. A QFutureWatcher applies completed snapshots on the owning thread. Destruction disconnects completion and waits for the independent collector before freeing it.

Additions:

- `busy` property and `busyChanged()` signal.
- `requestSample()` slot and `sampleFinished()` signal (including synchronous samples).
- `temperatureSampled(bool available, double celsius)` signal emitted for every applied collection, including unavailable temperature and paused UI. Existing `temperatureObserved(double)` still emits whenever a recognized CPU temperature is available. Connect the former to protection availability/freshness and the latter to legacy consumers as appropriate; do not count the same sample twice.
- Readings and summary include UTC ISO `updatedAt`. Each reading retains its exact filesystem `source` and `available` status.

## Correctness changes

- Read procfs contents before parsing: QTextStream::atEnd() can report an empty file for real zero-size kernel virtual files. The old real-memory test reproduced this failure.
- CPU utilization needs two real samples. Preserve complete 64-bit total/idle counters and exclude guest fields already counted in user/nice. Missing/malformed/reset counters establish a new baseline.
- Disk/network rates divide by measured steady-clock elapsed seconds. Network counters are parsed as integers before differencing, including values exceeding double's exact integer range. Missing devices drop their baselines.
- Powercap energy differences produce domain power and total package power, with published energy range used for wraparound; absent/partial package samples leave aggregate power unavailable.
- Follow hwmon class symlinks and identify channels using canonical device paths plus channel names, preserving simultaneous chips with identical names. CPU protection observes the hottest recognized CPU channel. Ignore faulted/disabled channels; legitimate subzero temperatures remain available.
- Current summaries clear missing values; removed channel statistics are discarded. Pause freezes the displayed model/history while collection and thermal availability continue.
- History and observed min/max/average use a rolling 90-sample window. Initial extrema derive from actual samples, rather than fabricated zeros or sensor bounds. Reset re-bases current statistics even while paused.
- Correct CPU throttle paths to sysfs rather than `/proc/cpu`.

## Verification

An isolated CMake harness compiled SensorService.cpp, SensorCollector.cpp and tst_sensors.cpp with `-Wall -Wextra -Wpedantic`, linked Qt6 Core/Concurrent/Test, and ran the suite successfully: **17 passed, zero failed** (including QtTest init/cleanup).

Coverage includes real host memory; empty/malformed roots; wide CPU counters and guest accounting; symlink discovery and same-name chips; removal/reappearance; paused/reset/bounded statistics; deterministic 2 s and 0.5 s rates; large integer network counters; energy wraparound; fault/enable flags; negative temperature; unavailable thermal signals during pause; queued async work, duplicate requests and immediate close during collection. No workload or benchmark was launched.

A separate Debug harness with `-fsanitize=address,undefined -fno-omit-frame-pointer` and leak detection also passed all 17 tests without sanitizer diagnostics.

The deterministic rate fixture supplies steady-clock collection timestamps directly to the non-QObject collector. The async fixture occupies the global pool briefly and confirms that requestSample returns without performing synchronous collection and that duplicate requests do not queue extra snapshots.

## Known limits

- Direct sysfs collection avoids libsensors process-global initialization and its cross-worker lifetime hazards. It currently supports the exported channels implemented here; libsensors label/config transformations, vendor GPU APIs, SMART, and arbitrary thermal-zone driver identification remain future work.
- Thermal CPU recognition is limited to coretemp, k10temp, zenpower, cpu_thermal and x86_pkg_temp names; unknown temperatures remain visible without being assigned as CPU protection sensors.
- The watcher joins an in-flight filesystem read on shutdown. Normal procfs/sysfs reads are small, but a kernel driver that blocks indefinitely cannot be forcibly interrupted by this worker design.
- Canonical hwmon paths preserve distinct devices and class symlink renumbering; physical topology/path changes across boots can change favorite identifiers.
- A lower powercap energy value with a valid range is treated as wraparound. Multiple wraps between slow samples cannot be distinguished from one wrap.
