# Grouped navigation and richer preferences

Date: 2026-11-05
Status: implemented and verified on this host

## Intent

Make large real-machine inventories and sensor sets readable at a glance,
following HWiNFO's information architecture: physical devices are the primary
group, sensor types are subsections, and groups can be collapsed. The hardware
tree receives the same treatment at category level. The settings page grows the
options that materially change monitoring and reading comfort.

## Goals

- Sensors page: device group → sensor-category subsection → sensor rows.
- Hardware page: collapsible category group → device rows.
- Preserve search, favorites, privacy, selection, copy and the existing
  `navigateTo` / objectName contracts.
- Keep a flat mode for users who prefer the previous list.
- Settings: sample interval, retained history length, temperature unit,
  diagnostics visibility, start page, reset-to-defaults.

## Non-goals

- No new privileged data sources, no fabricated devices or sensors.
- No multi-select filters or multi-series charts in this change.
- No migration of existing settings beyond preserving `schemaVersion`.

## Approaches considered

1. **QML-only grouping using the existing `group` field.** Smallest change, but
   `group` is a sensor type (Temperature, Cooling, …), not a device, so it
   cannot produce HWiNFO's device-first hierarchy.
2. **Collector adds device identity; QML builds a two-level grouped model.**
   Chosen: the collector already knows the hwmon chip, power supply, network
   interface and DRM card, so device identity is derived where the data is
   read. QML remains a pure presentation layer.
3. **Build a C++ tree model with QAbstractItemModel.** Strong for very large
   trees, but premature for roughly 80 sensor rows; it would move display
   ordering and collapse state into C++ and complicate the QML contract.

## Data model

`SensorSnapshot` gains `deviceIds` and `deviceNames`, keyed by sensor id.
`SensorService` exposes `deviceId` and `device` on every reading.

Derivation:

- CPU counters, cpufreq, powercap, thermal throttle, CPU hwmon chips → `cpu` / CPU.
- GPU driver hwmon and DRM counters → `gpu` / GPU.
- NVMe hwmon → `nvme` / NVMe.
- Known motherboard/super-I/O/thermal-zone chips → `motherboard` / Motherboard.
- Battery supplies → `battery` / Battery; disk stats → `storage` / Storage;
  network stats → `network` / Network; memory counters → `memory` / Memory;
  load average → `system` / System.
- Unknown hwmon chips keep a stable `hwmon:<canonical-path>` id and expose the
  chip name verbatim for diagnostics.

The existing `group` field remains the sensor category used for subsections.

## Interaction

- Grouped by default. A checkbox switches to the previous flat list.
- Device/category headers are keyboard-focusable buttons with a chevron and a
  channel count; click or keyboard toggles collapse.
- Search temporarily expands matching groups; clearing it restores the user's
  collapse state.
- Collapse state lives in the page for the session; it is intentionally not
  persisted.
- Hardware category rows show device counts; the device rows omit the repeated
  category label while grouped.
- Selecting a device in a collapsed category still updates the detail pane; the
  category can then be expanded to see the row.

## Settings

| Setting | Values | Effect |
| --- | --- | --- |
| Sample interval | 0.5 / 1 / 2 / 5 s | GUI sampling timer |
| History samples | 30 / 60 / 90 / 150 / 300 | SensorService history and per-channel statistics bound |
| Temperature unit | Celsius / Fahrenheit | Sensor cards, chart, sensor table, hardware temperature fields |
| Show diagnostics | on/off | Source/status/reason line under hardware fields; unavailable fields always explain themselves |
| Start page | seven pages | Initial `currentPage` |
| Reset preferences | confirm dialog | Restores defaults while preserving the schema version |

All values are validated in `Preferences`; invalid values fall back to defaults.
The sample interval and history bound are applied to the live services through
`Preferences::changed()`.

## Verification

- CTest 5/5, including new device-identity, configurable-history and preference
  round-trip/reset tests.
- `qmllint` exit 0 with no output.
- Offscreen and XWayland smoke tests exit 0 with zero QML warnings.
- Real screenshots show the hardware category tree and the sensor device/category
  hierarchy with actual host data.
