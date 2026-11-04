# Native TestRunner implementation report

## Scope and integration

Implemented in `native/core/TestRunner.h`, `native/core/TestRunner.cpp`, and
`native/tests/tst_testrunner.cpp`. No new source files or executable dependencies
were added to the application. The implementation requires Qt Core and Concurrent;
the test target additionally requires Qt Test and `/usr/bin/python3` for inert
process fixtures. No real stress or benchmark workload was run during validation.

Keep the existing `startStress`, `startBenchmark`, `stop`, `state`, `history`,
`capabilities`, and `checkTemperature` interfaces. The new integration should use:

```cpp
QObject::connect(&sensors, &neko::SensorService::temperatureSampled,
                 &tests, &neko::TestRunner::observeTemperature);
```

The optional fourth constructor argument overrides `/proc` for fixtures. Existing
three-argument construction remains source compatible.

## Behavior

- Tool launch uses asynchronous `QProcess::start`. A successful start return value
  means the request was accepted; actual launch failure is recorded through the
  event loop. Busy requests return false and emit a message without changing the
  current run or its history. Failed-to-start runs produce exactly one record.
- Memory stress reads current `MemAvailable` on a worker thread before launch.
  Unknown or insufficient availability refuses the run. The total VM allocation
  is bounded by the requested amount, 512 MiB, one quarter of availability, and
  availability minus 512 MiB of OS headroom. Each worker receives a share of that
  total budget. CPU-only stress does not require a memory reading.
- Child processes run in a new session/process group. Cancel, temperature stop,
  timeout, and descendant cleanup send TERM and escalate to KILL after 1.5 s.
  The saved process group remains tracked after the leader exits. Destruction
  waits for preparation and cleans up the process group with a bounded wait.
- Progress uses monotonic elapsed time and advances without child output. It
  stays below 100 until successful completion. Locale is fixed to C for parsing.
  Actual sysbench memory output (`MiB transferred (... MiB/sec)`) is supported.
- Every accepted attempt retains a bounded output tail, invocation, UTC start/end
  timestamps, elapsed duration, exit status, system/kernel, version availability,
  parameters, and terminal reason. Only successfully parsed, normally exited
  benchmarks have a score. Failed, canceled, missing-tool, and thermal stops have
  no score. History is bounded to 100 attempts; output to 64 Ki characters.
- `state.thermalProtection` is true only with a finite observation at most 3 s
  old. `thermalLimit` is 90 °C; `thermalStatus` is `fresh`, `stale`, or
  `unavailable`. Reaching the limit prevents launch or stops an active run.
  Losing a previously available sensor or letting it expire stops the active
  protected run. With no initial sensor, a run is explicitly unprotected.

## Validation

The test executable uses temporary Python fixtures that only print, sleep,
change signal handling, and fork a sleeping child. Checks cover command bounds,
OS memory reserve, expected output formats, missing tools, asynchronous failed
startup, busy rejection, output draining, locale, metadata/version, failed-exit
score refusal, silent progress, process-group cancellation, normal leader exit
with a remaining descendant, actual injected memory availability, unavailable
memory, sensor loss/staleness/limit, an expired idle sample, timeout, immediate
observer cancellation, restart independence, and destruction cleanup.

An isolated CMake harness builds with C++20, AUTOMOC, and
`-Wall -Wextra -Wpedantic`; the same suite also runs under AddressSanitizer and
UndefinedBehaviorSanitizer. Final command results are reported to the integrator.

## Known limits

Memory budgeting uses `/proc/meminfo` and does not additionally resolve cgroup
memory ceilings; allocation can still fail if availability changes before the
tool allocates. Tool versions come from their normal output banner and remain
explicitly unavailable when no banner is emitted. Process-group cleanup covers
descendants that retain the session/process group; a child deliberately creating
another session would require stronger cgroup containment. This Linux-targeted
implementation and its group-cleanup tests have not been validated on other OSes.
