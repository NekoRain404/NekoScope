# Rust boundary decision record

Status: accepted for the 0.2 milestone
Date: 2026-11-04
Scope: where Rust may enter NekoScope, and what must be true before it does

## Decision

The main application remains C++20 + Qt 6 Quick. **No Rust code is linked into
the application or its core library in this milestone.** Rust is adopted only
for a module that clears all four gates below. The first pre-qualified candidate
is a separate, privilege-separated read-only probe process, not a shared
library.

## Why not Rust in the main binary yet

- The Qt/QML boundary already owns object lifetime, signals and the data model.
  A second language inside the same process would create two representations of
  the same devices and two debugging stories.
- The current collectors are small, synchronous reads of procfs/sysfs. Rust's
  memory-safety advantage is real but does not offset FFI, build and packaging
  cost for that workload.
- Report encoding is already covered by Qt JSON and fully tested; swapping it
  for serde would duplicate a working data path for no user-visible gain.

## Adoption gates (all required)

1. **Concrete safety or ecosystem advantage** over a C++ implementation, stated
   in one paragraph before any code is written.
2. **Narrow, stable boundary**: a versioned C ABI of a few functions, or a
   process protocol with a documented message schema. No Qt objects, no
   `QVariant`, no shared mutable state across the boundary.
3. **Independent testability**: the module builds and tests with `cargo test`
   alone, including malformed/fuzzed inputs.
4. **Graceful absence**: if the module is unavailable, the application shows an
   explicit unavailable capability. It never falls back to a second
   implementation of the same data path.

## Candidate evaluation

| Candidate | Advantage | Boundary | Verdict |
| --- | --- | --- | --- |
| Privileged read-only probe (SMART/NVMe/SPD/DMI) | Memory-safe privileged binary, no UB on hostile device data, easy sandboxing and fuzzing | Versioned JSON over a Unix socket/D-Bus, or a small C ABI for a helper binary | **Selected as first candidate**, scheduled with Phase C. Requires Polkit and a security review before code. |
| Binary parsers (EDID, NVMe Identify, SPD) | Slice-safe parsing and cheap fuzzing | `staticlib` + C ABI | Deferred: `libdisplay-info` already covers EDID; revisit only if it becomes a hard dependency problem. |
| External tool output parsers (sysbench/stress-ng/fio) | Ergonomic text parsing | `cdylib` + C ABI | Rejected: C++ parser is small, already tested, and FFI adds more risk than it removes. |
| Core collectors / data model rewrite | none net | in-process | Rejected: duplicates the model and violates the single-owner rule. |
| Report serialization | serde ecosystem | in-process | Rejected: Qt JSON/CSV/HTML/Markdown already satisfy the contract. |

## Selected module contract (future work, not implemented)

- Name: `nekoscope-probe` (Rust binary, separate process).
- Read-only operations only: SMART health, NVMe identify, SPD/DIMM identity,
  DMI serial/UUID. No writes, no benchmark execution.
- Transport: Unix domain socket owned by root, peer credentials checked;
  Polkit authorizes the requesting session. Messages are versioned JSON with a
  maximum frame size; every response carries `status`, `source` and `timestamp`.
- Failure modes: helper absent, not authorized, device unsupported and read
  error are distinct states surfaced unchanged in the C++ model.
- Build: a `rust/probe/` Cargo package built independently; the core CMake build
  does not require Cargo. Packaging installs the helper as a separate optional
  unit with its own service file and permissions.

## Consequences

- The application stays buildable with only a C++20 toolchain and Qt 6.
- Rust work is isolated, testable and replaceable without touching QML.
- The user-facing capability set is unchanged until the probe exists; missing
  SMART/SPD data remains explicitly unavailable, never simulated.
